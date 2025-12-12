// Quantum 8-Ball Discord Bot
// Connects to the Gaming Module to consult the Oracle

package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/bwmarrin/discordgo"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// ------------------------------------------------------------------
// Oracle Client (talks to Gaming Module)
// ------------------------------------------------------------------

type OracleClient struct {
	conn       *grpc.ClientConn
	gamingAddr string
}

func NewOracleClient(gamingAddr string) (*OracleClient, error) {
	conn, err := grpc.Dial(gamingAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return nil, fmt.Errorf("failed to connect to gaming module: %w", err)
	}

	return &OracleClient{
		conn:       conn,
		gamingAddr: gamingAddr,
	}, nil
}

func (c *OracleClient) Close() error {
	if c.conn != nil {
		return c.conn.Close()
	}
	return nil
}

// AskOracle sends a question to the Gaming Module
// For now, this is a simplified version that simulates the Oracle response
func (c *OracleClient) AskOracle(question, userID string, mood int) (*OracleResponse, error) {
	// TODO: Use generated gRPC client to call Gaming.AskOracle
	// For now, simulate locally to demonstrate functionality

	prophecies := map[int][]string{
		0: { // Mysterious
			"The quantum realm whispers... yes ✨",
			"Signs point to affirmative 🌙",
			"The stars align in your favor ⭐",
			"Uncertain. Ask again when Mercury isn't retrograde 🌑",
			"The cosmos cannot reveal this 🔮",
			"Dark clouds obscure the answer ☁️",
			"The spirits say... unlikely 👻",
			"Absolutely not. The void has spoken 🕳️",
		},
		1: { // Sarcastic
			"Obviously yes, did you even need to ask? 🙄",
			"Yeah, sure, whatever 💅",
			"I guess... if you're lucky 🍀",
			"Ugh, try again later 😒",
			"I literally cannot even 💀",
			"Not a chance, buddy 🙃",
			"That's a hard no from me 🚫",
			"Are you kidding? No 😂",
		},
		2: { // Philosophical
			"In the infinite multiverse, this is true 🌌",
			"The wave function collapsed favorably 〰️",
			"Probability favors this outcome 📊",
			"Schrödinger would say both yes and no 🐱",
			"Some truths transcend binary answers ∞",
			"The universe gently suggests otherwise 🌍",
			"Entropy increases against this outcome 🔥",
			"In no timeline does this occur ⏰",
		},
		3: { // Chaotic
			"ABSOLUTELY! *explodes* 💥",
			"YES! But also maybe no? YES! 🎭",
			"The dice gods approve 🎲🎲🎲",
			"ERROR 404: FATE NOT FOUND 🤖",
			"¯\\_(ツ)_/¯ ¯\\_(ツ)_/¯ ¯\\_(ツ)_/¯",
			"NO! And your question was bad! 😤",
			"lol no. also lmao. also no. 💀",
			"THE VOID CONSUMES YOUR HOPES 🕳️",
		},
	}

	// Simulate 3-qubit measurement
	outcome := int(time.Now().UnixNano() % 8)
	moodIndex := mood % 4

	confidence := []float64{0.95, 0.85, 0.75, 0.50, 0.40, 0.35, 0.25, 0.15}[outcome]

	return &OracleResponse{
		Prophecy:     prophecies[moodIndex][outcome],
		OutcomeIndex: outcome,
		Confidence:   confidence,
		QuantumState: fmt.Sprintf("θ=%.3f, φ=%.3f", float64(outcome)*0.449, float64(outcome)*0.785),
		Timestamp:    time.Now().Unix(),
	}, nil
}

type OracleResponse struct {
	Prophecy     string
	OutcomeIndex int
	Confidence   float64
	QuantumState string
	Timestamp    int64
}

// ------------------------------------------------------------------
// Discord Bot
// ------------------------------------------------------------------

type Bot struct {
	session      *discordgo.Session
	oracleClient *OracleClient
}

func NewBot(token string, oracleClient *OracleClient) (*Bot, error) {
	session, err := discordgo.New("Bot " + token)
	if err != nil {
		return nil, fmt.Errorf("failed to create Discord session: %w", err)
	}

	bot := &Bot{
		session:      session,
		oracleClient: oracleClient,
	}

	// Register handlers
	session.AddHandler(bot.handleReady)
	session.AddHandler(bot.handleMessageCreate)
	session.AddHandler(bot.handleInteractionCreate)

	return bot, nil
}

func (b *Bot) Start() error {
	if err := b.session.Open(); err != nil {
		return fmt.Errorf("failed to open Discord session: %w", err)
	}

	// Register slash commands
	commands := []*discordgo.ApplicationCommand{
		{
			Name:        "8ball",
			Description: "Ask the Quantum Oracle a yes/no question",
			Options: []*discordgo.ApplicationCommandOption{
				{
					Type:        discordgo.ApplicationCommandOptionString,
					Name:        "question",
					Description: "Your question for the Oracle",
					Required:    true,
				},
				{
					Type:        discordgo.ApplicationCommandOptionInteger,
					Name:        "mood",
					Description: "The Oracle's mood",
					Required:    false,
					Choices: []*discordgo.ApplicationCommandOptionChoice{
						{Name: "🔮 Mysterious", Value: 0},
						{Name: "🙄 Sarcastic", Value: 1},
						{Name: "🌌 Philosophical", Value: 2},
						{Name: "💥 Chaotic", Value: 3},
					},
				},
			},
		},
		{
			Name:        "oracle",
			Description: "Alias for /8ball",
			Options: []*discordgo.ApplicationCommandOption{
				{
					Type:        discordgo.ApplicationCommandOptionString,
					Name:        "question",
					Description: "Your question for the Oracle",
					Required:    true,
				},
			},
		},
	}

	for _, cmd := range commands {
		_, err := b.session.ApplicationCommandCreate(b.session.State.User.ID, "", cmd)
		if err != nil {
			log.Printf("⚠️ Failed to register command %s: %v", cmd.Name, err)
		}
	}

	return nil
}

func (b *Bot) Stop() error {
	return b.session.Close()
}

func (b *Bot) handleReady(s *discordgo.Session, r *discordgo.Ready) {
	log.Printf("🎱 Quantum Oracle Bot is online as %s#%s", r.User.Username, r.User.Discriminator)

	// Set status
	s.UpdateGameStatus(0, "🎱 /8ball to consult the Oracle")
}

func (b *Bot) handleMessageCreate(s *discordgo.Session, m *discordgo.MessageCreate) {
	// Ignore bot messages
	if m.Author.Bot {
		return
	}

	// Handle !8ball command (legacy text command)
	if strings.HasPrefix(strings.ToLower(m.Content), "!8ball ") {
		question := strings.TrimPrefix(m.Content, "!8ball ")
		question = strings.TrimPrefix(question, "!8Ball ")

		response, err := b.oracleClient.AskOracle(question, m.Author.ID, 0)
		if err != nil {
			s.ChannelMessageSend(m.ChannelID, "❌ The Oracle is unavailable...")
			return
		}

		embed := b.createOracleEmbed(question, response, m.Author)
		s.ChannelMessageSendEmbed(m.ChannelID, embed)
	}
}

func (b *Bot) handleInteractionCreate(s *discordgo.Session, i *discordgo.InteractionCreate) {
	if i.Type != discordgo.InteractionApplicationCommand {
		return
	}

	data := i.ApplicationCommandData()

	switch data.Name {
	case "8ball", "oracle":
		b.handleOracleCommand(s, i)
	}
}

func (b *Bot) handleOracleCommand(s *discordgo.Session, i *discordgo.InteractionCreate) {
	// Defer response (Oracle needs time to "think")
	s.InteractionRespond(i.Interaction, &discordgo.InteractionResponse{
		Type: discordgo.InteractionResponseDeferredChannelMessageWithSource,
	})

	data := i.ApplicationCommandData()

	var question string
	mood := 0 // Default: Mysterious

	for _, opt := range data.Options {
		switch opt.Name {
		case "question":
			question = opt.StringValue()
		case "mood":
			mood = int(opt.IntValue())
		}
	}

	// Consult the Oracle
	response, err := b.oracleClient.AskOracle(question, i.Member.User.ID, mood)
	if err != nil {
		s.InteractionResponseEdit(i.Interaction, &discordgo.WebhookEdit{
			Content: strPtr("❌ The Oracle is unavailable: " + err.Error()),
		})
		return
	}

	embed := b.createOracleEmbed(question, response, i.Member.User)
	s.InteractionResponseEdit(i.Interaction, &discordgo.WebhookEdit{
		Embeds: &[]*discordgo.MessageEmbed{embed},
	})
}

func (b *Bot) createOracleEmbed(question string, response *OracleResponse, user *discordgo.User) *discordgo.MessageEmbed {
	// Color based on confidence
	var color int
	switch {
	case response.Confidence >= 0.75:
		color = 0x00FF00 // Green - positive
	case response.Confidence >= 0.50:
		color = 0xFFFF00 // Yellow - neutral
	case response.Confidence >= 0.25:
		color = 0xFFA500 // Orange - uncertain
	default:
		color = 0xFF0000 // Red - negative
	}

	return &discordgo.MessageEmbed{
		Title:       "🎱 The Quantum Oracle Speaks",
		Description: fmt.Sprintf("**%s**", response.Prophecy),
		Color:       color,
		Fields: []*discordgo.MessageEmbedField{
			{
				Name:   "❓ Question",
				Value:  question,
				Inline: false,
			},
			{
				Name:   "📊 Confidence",
				Value:  fmt.Sprintf("%.0f%%", response.Confidence*100),
				Inline: true,
			},
			{
				Name:   "⚛️ Quantum State",
				Value:  response.QuantumState,
				Inline: true,
			},
			{
				Name:   "🎲 Outcome",
				Value:  fmt.Sprintf("%d/7", response.OutcomeIndex),
				Inline: true,
			},
		},
		Footer: &discordgo.MessageEmbedFooter{
			Text:    fmt.Sprintf("Asked by %s • Powered by 3-qubit superposition", user.Username),
			IconURL: user.AvatarURL("32"),
		},
		Timestamp: time.Now().Format(time.RFC3339),
	}
}

func strPtr(s string) *string {
	return &s
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------

func main() {
	token := flag.String("token", "", "Discord bot token")
	gamingAddr := flag.String("gaming-addr", "gaming:50061", "Gaming module address")
	flag.Parse()

	// Check for token in environment
	if *token == "" {
		*token = os.Getenv("DISCORD_TOKEN")
	}

	if *token == "" {
		log.Fatal("Discord token required. Set DISCORD_TOKEN env var or use -token flag")
	}

	// Connect to Gaming Module
	oracleClient, err := NewOracleClient(*gamingAddr)
	if err != nil {
		log.Printf("⚠️ Warning: Could not connect to Gaming module: %v", err)
		log.Printf("⚠️ Running in standalone mode with local Oracle")
		oracleClient = &OracleClient{} // Empty client, will use local fallback
	}
	defer oracleClient.Close()

	// Create and start bot
	bot, err := NewBot(*token, oracleClient)
	if err != nil {
		log.Fatalf("Failed to create bot: %v", err)
	}

	if err := bot.Start(); err != nil {
		log.Fatalf("Failed to start bot: %v", err)
	}
	defer bot.Stop()

	log.Println("🎱 Quantum Oracle Bot is running. Press Ctrl+C to stop.")

	// Wait for interrupt signal
	sc := make(chan os.Signal, 1)
	signal.Notify(sc, syscall.SIGINT, syscall.SIGTERM, os.Interrupt)
	<-sc

	log.Println("🎱 Shutting down...")

	_ = context.Background() // For future use with gRPC context
}
