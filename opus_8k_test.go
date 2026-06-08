//go:build (amd64 && windows) || (amd64 && linux)

package audio_transcoder

import "testing"

// Regression test for JT1078 AAC voice devices, whose audio is 8000 Hz mono.
//
// The Opus encoder hardcoded the per-channel frame size to 960 samples. That is a
// 20ms frame only at 48 kHz; at 8 kHz it is 120 ms. libopus accepts up to 120ms by
// splitting internally, so the encoder is not silent — but WebRTC negotiates 20ms
// Opus (minptime=10) and browser stacks do not reliably decode 120ms packets, so the
// audio does not play. The fix derives the frame size from the sample rate so every
// rate produces standard 20ms frames.
//
// Unfixed: 8kHz -> 120ms/frame (~8 packets/sec) -> fails the duration assertion.
// Fixed:   8kHz -> 20ms/frame  (~50 packets/sec) -> passes.
func TestOpusEncode8kHzMonoProducesOutput(t *testing.T) {
	enc, err := FindEncoder("OPUS", 8000, 1)
	if enc == nil {
		t.Fatalf("FindEncoder(OPUS,8000,1) returned nil: %v", err)
	}
	oe := enc.(*OpusEncoder)
	if _, err := oe.Create(8000, 1); err != nil {
		t.Fatalf("Create(8000,1): %v", err)
	}
	defer oe.Destroy()

	// WebRTC Opus must use frames <= 60ms (it negotiates 20ms). 120ms is out of spec.
	if d := oe.PacketDurationMS(); d <= 0 || d > 60 {
		t.Fatalf("Opus frame duration %dms is not WebRTC-compatible at 8kHz (want 20ms, <=60ms)", d)
	}

	// 1 second of 8 kHz mono 16-bit PCM = 8000 samples = 16000 bytes.
	// A simple non-silent sawtooth so the encoder has real signal to encode.
	pcm := make([]byte, 16000)
	for i := 0; i < 8000; i++ {
		v := int16((i%160)*400 - 16000)
		pcm[i*2] = byte(v)
		pcm[i*2+1] = byte(v >> 8)
	}

	var packets, bytesOut int
	if _, err := oe.Encode(pcm, func(b []byte) { packets++; bytesOut += len(b) }); err != nil {
		t.Fatalf("Encode: %v", err)
	}

	if packets == 0 || bytesOut == 0 {
		t.Fatalf("expected Opus output for 8 kHz mono input, got %d packets / %d bytes", packets, bytesOut)
	}
	// ~1s of audio in 20ms frames is ~50 packets; <16 means frames are >60ms.
	if packets < 16 {
		t.Fatalf("got %d packets for 1s of 8kHz audio => frames >60ms (not WebRTC-compatible); want ~50", packets)
	}
	t.Logf("8 kHz mono Opus OK: %d packets, %d bytes, %dms/frame", packets, bytesOut, oe.PacketDurationMS())
}
