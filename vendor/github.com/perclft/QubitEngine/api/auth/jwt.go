package auth

import (
	"context"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

func getJwtSecret() ([]byte, error) {
	secret := os.Getenv("QUBIT_ENGINE_JWT_SECRET")
	if secret == "" {
		if os.Getenv("QUBIT_ENGINE_SKIP_AUTH") == "1" {
			return []byte("dummy-skip-secret"), nil
		}
		return nil, fmt.Errorf("QUBIT_ENGINE_JWT_SECRET is required but not set")
	}
	return []byte(secret), nil
}

func GenerateToken(userID string, expiry time.Duration) (string, error) {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.RegisteredClaims{
		Subject:   userID,
		Issuer:    "qubit-engine",
		Audience:  jwt.ClaimStrings{"qubit-engine-api"},
		ExpiresAt: jwt.NewNumericDate(time.Now().Add(expiry)),
		IssuedAt:  jwt.NewNumericDate(time.Now()),
	})
	secret, err := getJwtSecret()
	if err != nil {
		return "", err
	}
	return token.SignedString(secret)
}

func ValidateToken(tokenString string) (string, error) {
	if os.Getenv("QUBIT_ENGINE_SKIP_AUTH") == "1" {
		return "skip-auth-user", nil
	}

	var claims jwt.RegisteredClaims
	token, err := jwt.ParseWithClaims(tokenString, &claims, func(t *jwt.Token) (interface{}, error) {
		if _, ok := t.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", t.Header["alg"])
		}
		return getJwtSecret()
	})

	if err != nil {
		return "", err
	}

	if token.Valid {
		if claims.Issuer != "qubit-engine" {
			return "", fmt.Errorf("invalid issuer")
		}
		
		if len(claims.Audience) == 0 || claims.Audience[0] != "qubit-engine-api" {
			return "", fmt.Errorf("invalid audience")
		}

		return claims.Subject, nil
	}

	return "", fmt.Errorf("invalid token claims")
}

func ExtractTokenFromContext(ctx context.Context) (string, error) {
	if os.Getenv("QUBIT_ENGINE_SKIP_AUTH") == "1" {
		return "dummy-skip-token", nil
	}

	md, ok := metadata.FromIncomingContext(ctx)
	if !ok {
		return "", status.Errorf(codes.Unauthenticated, "missing metadata")
	}

	authHeader, ok := md["authorization"]
	if !ok || len(authHeader) == 0 {
		return "", status.Errorf(codes.Unauthenticated, "missing authorization header")
	}

	parts := strings.Split(authHeader[0], " ")
	if len(parts) != 2 || strings.ToLower(parts[0]) != "bearer" {
		return "", status.Errorf(codes.Unauthenticated, "invalid authorization format")
	}

	return parts[1], nil
}
