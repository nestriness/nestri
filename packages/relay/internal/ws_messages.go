package relay

import (
	"bytes"
	"compress/gzip"
	"encoding/json"
	"fmt"
	"github.com/pion/webrtc/v4"
	"time"
)

// WSMessageBase is the base type for WebSocket messages.
type WSMessageBase struct {
	PayloadType string `json:"payload_type"`
}

// WSMessageLog represents a log message.
type WSMessageLog struct {
	WSMessageBase
	Level   string `json:"level"`
	Message string `json:"message"`
	Time    string `json:"time"`
}

// WSMessageMetrics represents a metrics/heartbeat message.
type WSMessageMetrics struct {
	WSMessageBase
	UsageCPU        float64 `json:"usage_cpu"`
	UsageMemory     float64 `json:"usage_memory"`
	Uptime          uint64  `json:"uptime"`
	PipelineLatency float64 `json:"pipeline_latency"`
}

// WSMessageICECandidate represents an ICE candidate message.
type WSMessageICECandidate struct {
	WSMessageBase
	Candidate webrtc.ICECandidateInit `json:"candidate"`
}

// WSMessageSDP represents an SDP message.
type WSMessageSDP struct {
	WSMessageBase
	SDP webrtc.SessionDescription `json:"sdp"`
}

// JoinerType is an enum for the type of incoming room joiner
type JoinerType int

const (
	JoinerNode JoinerType = iota
	JoinerClient
)

func (jt *JoinerType) String() string {
	switch *jt {
	case JoinerNode:
		return "node"
	case JoinerClient:
		return "client"
	default:
		return "unknown"
	}
}

// WSMessageJoin is used to tell us that either participant or ingest wants to join the room
type WSMessageJoin struct {
	WSMessageBase
	JoinerType JoinerType `json:"joiner_type"`
}

// AnswerType is an enum for the type of answer, signaling Room state for a joiner
type AnswerType int

const (
	AnswerOffline AnswerType = iota // For participant/client, when the room is offline without stream
	AnswerInUse                     // For ingest/node joiner, when the room is already in use by another ingest/node
	AnswerOK                        // For both, when the join request is handled successfully
)

// WSMessageAnswer is used to send the answer to a join request
type WSMessageAnswer struct {
	WSMessageBase
	AnswerType AnswerType `json:"answer_type"`
}

// EncodeMessage encodes a message to be sent over the websocket with gzip compression
func EncodeMessage(msg interface{}) ([]byte, error) {
	// Marshal the message to JSON
	data, err := json.Marshal(msg)
	if err != nil {
		return nil, fmt.Errorf("failed to encode message: %w", err)
	}

	// Gzip compress the JSON
	var compressedData bytes.Buffer
	writer := gzip.NewWriter(&compressedData)
	_, err = writer.Write(data)
	if err != nil {
		return nil, fmt.Errorf("failed to compress message: %w", err)
	}
	if err := writer.Close(); err != nil {
		return nil, fmt.Errorf("failed to finalize compression: %w", err)
	}

	return compressedData.Bytes(), nil
}

// DecodeMessage decodes a message received over the websocket with gzip decompression
func DecodeMessage(data []byte, target interface{}) error {
	// Gzip decompress the data
	reader, err := gzip.NewReader(bytes.NewReader(data))
	if err != nil {
		return fmt.Errorf("failed to initialize decompression: %w", err)
	}
	defer func(reader *gzip.Reader) {
		if err = reader.Close(); err != nil {
			fmt.Printf("failed to close reader: %v\n", err)
		}
	}(reader)

	// Decode the JSON
	err = json.NewDecoder(reader).Decode(target)
	if err != nil {
		return fmt.Errorf("failed to decode message: %w", err)
	}

	return nil
}

// SendLogMessage sends a log message to the given WebSocket connection.
func (ws *SafeWebSocket) SendLogMessage(level, message string) error {
	msg := WSMessageLog{
		WSMessageBase: WSMessageBase{PayloadType: "log"},
		Level:         level,
		Message:       message,
		Time:          time.Now().Format(time.RFC3339),
	}
	encoded, err := EncodeMessage(msg)
	if err != nil {
		return fmt.Errorf("failed to encode log message: %w", err)
	}

	return ws.WriteBinary(encoded)
}

// SendMetricsMessage sends a metrics message to the given WebSocket connection.
func (ws *SafeWebSocket) SendMetricsMessage(usageCPU, usageMemory float64, uptime uint64, pipelineLatency float64) error {
	msg := WSMessageMetrics{
		WSMessageBase:   WSMessageBase{PayloadType: "metrics"},
		UsageCPU:        usageCPU,
		UsageMemory:     usageMemory,
		Uptime:          uptime,
		PipelineLatency: pipelineLatency,
	}
	encoded, err := EncodeMessage(msg)
	if err != nil {
		return fmt.Errorf("failed to encode metrics message: %w", err)
	}

	return ws.WriteBinary(encoded)
}

// SendICECandidateMessage sends an ICE candidate message to the given WebSocket connection.
func (ws *SafeWebSocket) SendICECandidateMessage(candidate webrtc.ICECandidateInit) error {
	msg := WSMessageICECandidate{
		WSMessageBase: WSMessageBase{PayloadType: "ice"},
		Candidate:     candidate,
	}
	encoded, err := EncodeMessage(msg)
	if err != nil {
		return fmt.Errorf("failed to encode ICE candidate message: %w", err)
	}

	return ws.WriteBinary(encoded)
}

// SendSDPMessage sends an SDP message to the given WebSocket connection.
func (ws *SafeWebSocket) SendSDPMessage(sdp webrtc.SessionDescription) error {
	msg := WSMessageSDP{
		WSMessageBase: WSMessageBase{PayloadType: "sdp"},
		SDP:           sdp,
	}
	encoded, err := EncodeMessage(msg)
	if err != nil {
		return fmt.Errorf("failed to encode SDP message: %w", err)
	}

	return ws.WriteBinary(encoded)
}

// SendAnswerMessage sends an answer message to the given WebSocket connection.
func (ws *SafeWebSocket) SendAnswerMessage(answer AnswerType) error {
	msg := WSMessageAnswer{
		WSMessageBase: WSMessageBase{PayloadType: "answer"},
		AnswerType:    answer,
	}
	encoded, err := EncodeMessage(msg)
	if err != nil {
		return fmt.Errorf("failed to encode answer message: %w", err)
	}

	return ws.WriteBinary(encoded)
}
