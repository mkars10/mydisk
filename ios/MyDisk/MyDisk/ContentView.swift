//
//  ContentView.swift
//  MyDisk
//
//  Created by Michael Karsten on 9/28/25.
//

import SwiftUI

struct ContentView: View {
    @State private var isRunning = false
    @State private var signalStrength = 50.0

    var body: some View {
        NavigationStack {
            VStack(spacing: 16) {
                Text("Welcome to MyDisk!")
                    .font(.title.bold())

                Spacer()

                SignalStrengthBar(value: signalStrength)

                Button(isRunning ? "Stop" : "Start") {
                    isRunning.toggle()
                }
                .frame(maxWidth: .infinity)
                .padding()
                .background(isRunning ? .red : .green)
                .foregroundStyle(.white)
                .clipShape(RoundedRectangle(cornerRadius: 12))
            }
            .padding()
        }
    }
}

struct SignalStrengthBar: View {
    let value: Double

    var body: some View {
        let clampedValue = min(max(value, 0), 100)

        VStack(alignment: .leading, spacing: 6) {
            Text("Signal Strength")
                .font(.headline)

            GeometryReader { proxy in
                let fillWidth = proxy.size.width * (clampedValue / 100)

                RoundedRectangle(cornerRadius: 12)
                    .fill(.secondary.opacity(0.2))
                    .overlay {
                        LinearGradient(
                            colors: [.red, .green],
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                        .mask {
                            HStack(spacing: 0) {
                                Rectangle()
                                    .frame(width: fillWidth)
                                Spacer(minLength: 0)
                            }
                        }
                    }
            }
            .frame(height: 44)

            HStack {
                Text("0")
                Spacer()
                Text("100")
            }
            .font(.caption)
            .foregroundStyle(.secondary)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Signal Strength")
        .accessibilityValue("\(clampedValue, format: .number.precision(.fractionLength(0))) out of 100")
    }
}

#Preview {
    ContentView()
}
