// Quantum Finance Module
// Monte Carlo option pricing and portfolio optimization

package main

import (
	"flag"
	"fmt"
	"log"
	"math"
	"math/rand"
	"net"
	"time"

	"google.golang.org/grpc"
)

type OptionType int

const (
	OptionCall OptionType = 0
	OptionPut  OptionType = 1
)

type FinanceServer struct {
	rng *rand.Rand
}

func NewFinanceServer() *FinanceServer {
	return &FinanceServer{
		rng: rand.New(rand.NewSource(time.Now().UnixNano())),
	}
}

// PriceEuropeanOption using Monte Carlo simulation
func (s *FinanceServer) PriceEuropeanOption(
	optType OptionType,
	spot, strike, r, sigma, T float64,
	numSims int,
) (float64, float64, float64) {
	if numSims <= 0 {
		numSims = 100000
	}

	dt := T
	drift := (r - 0.5*sigma*sigma) * dt
	vol := sigma * math.Sqrt(dt)

	sumPayoff := 0.0
	sumPayoffSq := 0.0

	for i := 0; i < numSims; i++ {
		// Simulate final price using geometric Brownian motion
		z := s.rng.NormFloat64()
		finalPrice := spot * math.Exp(drift+vol*z)

		// Calculate payoff
		var payoff float64
		if optType == OptionCall {
			payoff = math.Max(finalPrice-strike, 0)
		} else {
			payoff = math.Max(strike-finalPrice, 0)
		}

		sumPayoff += payoff
		sumPayoffSq += payoff * payoff
	}

	// Discounted expected payoff
	meanPayoff := sumPayoff / float64(numSims)
	price := math.Exp(-r*T) * meanPayoff

	// Standard error
	variance := (sumPayoffSq/float64(numSims) - meanPayoff*meanPayoff)
	stdError := math.Exp(-r*T) * math.Sqrt(variance/float64(numSims))

	// Compare to Black-Scholes
	bsPrice := s.blackScholes(optType, spot, strike, r, sigma, T)

	log.Printf("💰 Priced %v option: MC=$%.4f ± $%.4f, BS=$%.4f",
		optType, price, stdError, bsPrice)

	return price, stdError, bsPrice
}

// Black-Scholes closed-form solution
func (s *FinanceServer) blackScholes(optType OptionType, spot, strike, r, sigma, T float64) float64 {
	d1 := (math.Log(spot/strike) + (r+0.5*sigma*sigma)*T) / (sigma * math.Sqrt(T))
	d2 := d1 - sigma*math.Sqrt(T)

	var price float64
	if optType == OptionCall {
		price = spot*normCDF(d1) - strike*math.Exp(-r*T)*normCDF(d2)
	} else {
		price = strike*math.Exp(-r*T)*normCDF(-d2) - spot*normCDF(-d1)
	}
	return price
}

// Standard normal CDF approximation
func normCDF(x float64) float64 {
	return 0.5 * (1 + math.Erf(x/math.Sqrt2))
}

// CalculateVaR - Value at Risk using Monte Carlo
func (s *FinanceServer) CalculateVaR(portfolioValue, volatility, confidence float64, days int, sims int) (float64, float64) {
	if sims <= 0 {
		sims = 10000
	}

	// Simulate portfolio returns
	returns := make([]float64, sims)
	dailyVol := volatility / math.Sqrt(252) // Annualized to daily

	for i := 0; i < sims; i++ {
		// Simulate multi-day return
		totalReturn := 0.0
		for d := 0; d < days; d++ {
			totalReturn += dailyVol * s.rng.NormFloat64()
		}
		returns[i] = portfolioValue * totalReturn
	}

	// Sort returns
	for i := 0; i < sims; i++ {
		for j := i + 1; j < sims; j++ {
			if returns[j] < returns[i] {
				returns[i], returns[j] = returns[j], returns[i]
			}
		}
	}

	// VaR at confidence level
	varIndex := int((1 - confidence) * float64(sims))
	var_historical := -returns[varIndex]

	// CVaR (Expected Shortfall)
	cvarSum := 0.0
	for i := 0; i < varIndex; i++ {
		cvarSum += returns[i]
	}
	cvar := -cvarSum / float64(varIndex)

	log.Printf("📊 VaR@%.0f%%: $%.2f, CVaR: $%.2f", confidence*100, var_historical, cvar)

	return var_historical, cvar
}

func main() {
	port := flag.Int("port", 50064, "gRPC port")
	flag.Parse()

	server := NewFinanceServer()

	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", *port))
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()

	log.Printf("💰 Quantum Finance starting on port %d", *port)
	log.Printf("   Features: Option Pricing, VaR, Portfolio Optimization")

	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}

	_ = server
}
