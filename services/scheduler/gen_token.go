package main
import (
    "fmt"
    "time"
    "github.com/golang-jwt/jwt/v5"
)
func main() {
    token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.RegisteredClaims{
        Subject:   "test-user",
        ExpiresAt: jwt.NewNumericDate(time.Now().Add(10 * 365 * 24 * time.Hour)),
    })
    s, _ := token.SignedString([]byte("qubit-engine-development-secret-12345"))
    fmt.Println(s)
}
