package main
import (
    "fmt"
    "time"
    "os"
    "github.com/golang-jwt/jwt/v5"
)
func main() {
    secret := os.Getenv("QUBIT_ENGINE_JWT_SECRET")
    if secret == "" {
        panic("QUBIT_ENGINE_JWT_SECRET must be set")
    }
    token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.RegisteredClaims{
        Subject:   "test-user",
        Issuer:    "qubit-engine",
        Audience:  jwt.ClaimStrings{"qubit-engine-api"},
        ExpiresAt: jwt.NewNumericDate(time.Now().Add(10 * 365 * 24 * time.Hour)),
    })
    s, _ := token.SignedString([]byte(secret))
    fmt.Println(s)
}
