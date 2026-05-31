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

func getJwtSecret() []byte {
	secret := os.Getenv("QUBIT_ENGINE_JWT_SECRET")
	if secret == "" {
		if os.Getenv("QUBIT_ENGINE_SKIP_AUTH") == "1" {
			return []byte("dummy-skip-secret")
		}
		panic("QUBIT_ENGINE_JWT_SECRET is required but not set")
	}
	return []byte(secret)
}

func GenerateToken(userID string, expiry time.Duration) (string, error) {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.RegisteredClaims{
		Subject:   userID,
		Issuer:    "qubit-engine",
		Audience:  jwt.ClaimStrings{"qubit-engine-api"},
		ExpiresAt: jwt.NewNumericDate(time.Now().Add(expiry)),
		IssuedAt:  jwt.NewNumericDate(time.Now()),
	})
	return token.SignedString(getJwtSecret())
}

func ValidateToken(tokenString string) (string, error) {
	if os.Getenv("QUBIT_ENGINE_SKIP_AUTH") == "1" {
		return "skip-auth-user", nil
	}

	token, err := jwt.Parse(tokenString, func(t *jwt.Token) (interface{}, error) {
		if _, ok := t.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", t.Header["alg"])
		}
		return getJwtSecret(), nil
	})

	if err != nil {
		return "", err
	}

	if claims, ok := token.Claims.(jwt.MapClaims); ok && token.Valid {
		iss, err := claims.GetIssuer()
		if err != nil || iss != "qubit-engine" {
			return "", fmt.Errorf("invalid issuer")
		}
		
		aud, err := claims.GetAudience()
		if err != nil || len(aud) == 0 || aud[0] != "qubit-engine-api" {
			return "", fmt.Errorf("invalid audience")
		}

		sub, _ := claims.GetSubject()
		return sub, nil
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
