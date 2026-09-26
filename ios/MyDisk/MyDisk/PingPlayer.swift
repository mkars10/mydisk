import AVFAudio

@MainActor
final class PingPlayer {
    static let shared = PingPlayer()

    private static let sampleRate = 44_100.0
    private static let duration = 0.08
    private static let frequency: Float = 1_100

    private let engine = AVAudioEngine()
    private let player = AVAudioPlayerNode()
    private let pingBuffer: AVAudioPCMBuffer?

    private init() {
        let frameCount = AVAudioFrameCount(Self.sampleRate * Self.duration)

        guard let format = AVAudioFormat(
            standardFormatWithSampleRate: Self.sampleRate,
            channels: 1
        ), let buffer = AVAudioPCMBuffer(
            pcmFormat: format,
            frameCapacity: frameCount
        ), let samples = buffer.floatChannelData?.pointee else {
            pingBuffer = nil
            return
        }

        buffer.frameLength = frameCount

        for frame in 0..<Int(frameCount) {
            let progress = Float(frame) / Float(frameCount)
            let envelope = (1 - progress) * 0.2
            let phase = 2 * .pi * Self.frequency * Float(frame) / Float(Self.sampleRate)
            samples[frame] = sinf(phase) * envelope
        }

        pingBuffer = buffer
        engine.attach(player)
        engine.connect(player, to: engine.mainMixerNode, format: format)
    }

    func play() {
        guard let pingBuffer else { return }

        do {
            let session = AVAudioSession.sharedInstance()
            try session.setCategory(.ambient)
            try session.setActive(true)

            if !engine.isRunning {
                try engine.start()
            }

            player.stop()
            player.scheduleBuffer(pingBuffer, at: nil)
            player.play()
        } catch {
            return
        }
    }
}
