//
//  ContentView.swift
//  MyDisk
//
//  Created by Michael Karsten on 9/28/25.
//

import Observation
import SwiftUI

struct ContentView: View {
    @State private var isRunning = false
    @State private var signalStrengthModel = SignalStrengthModel()
    @State private var disks: [Disk] = []
    @State private var selectedDisk = ""

    private let signalStrengthSource: any SignalStrengthSource = MockSignalStrengthSource()

    var body: some View {
        NavigationStack {
            VStack(spacing: 16) {
                Text("Disk Finder")
                    .font(.title.bold())

                Picker("Disk", selection: $selectedDisk) {
                    ForEach(disks) { disk in
                            Text(disk.name)
                                .tag(disk.name) // The tag must match the data type of $selectedDisk
                        }
                }
                .pickerStyle(.menu)

                NavigationLink {
                    StashView()
                } label: {
                    Text("Manage Stash")
                }
                .buttonStyle(.bordered)

                Spacer()

                SignalStrengthBar(value: signalStrengthModel.value)

                Button(isRunning ? "Stop Scanning" : "Start Scanning") {
                    isRunning.toggle()
                }
                .frame(maxWidth: .infinity)
                .padding()
                .background(isRunning ? .red : .green)
                .foregroundStyle(.white)
                .clipShape(RoundedRectangle(cornerRadius: 12))
            }
            .padding()
            .onAppear {
                disks = DiskStore.load()
                if let firstDisk = disks.first {
                    selectedDisk = firstDisk.name
                }
            }
        }
        .task(id: isRunning) {
            guard isRunning else {
                signalStrengthModel.reset()
                return
            }

            await signalStrengthSource.start(updating: signalStrengthModel)
        }
    }
}

@MainActor
@Observable
final class SignalStrengthModel {
    var value = 0.0

    func reset() {
        value = 0
    }

    func record(_ newValue: Double) {
        value = min(max(newValue, 0), 100)
        PingPlayer.shared.play()
    }
}

@MainActor
protocol SignalStrengthSource {
    func start(updating model: SignalStrengthModel) async
}

@MainActor
struct MockSignalStrengthSource: SignalStrengthSource {
    func start(updating model: SignalStrengthModel) async {
        while !Task.isCancelled {
            do {
                try await Task.sleep(for: .milliseconds(500))
            } catch {
                return
            }

            // Replace this source with the Bluetooth reader when it is available.
            model.record(Double.random(in: 0...100))
        }
    }
}

struct StashView: View {
    @State private var disks: [Disk] = []
    @State private var isShowingAddForm = false

    var body: some View {
        VStack(spacing: 16) {
            List {
                // 1. Table Header Row (Simulated)
                HStack {
                    Text("Disk Name").bold().frame(maxWidth: .infinity, alignment: .leading)
                    Text("Type").bold().frame(maxWidth: .infinity, alignment: .leading)
                    Text("Flight Numbers").bold().frame(maxWidth: .infinity, alignment: .leading)
                }
                .listRowBackground(Color.clear) // Keeps the header flat
                
                // 2. Data Rows
                ForEach(disks) { disk in
                    HStack {
                        Text(disk.name).frame(maxWidth: .infinity, alignment: .leading)
                        Text(disk.type).frame(maxWidth: .infinity, alignment: .leading)
                        Text(disk.flightNumbers.map(String.init).joined(separator: ", ")).frame(maxWidth: .infinity, alignment: .leading)
                    }
                    // 3. Attach the native iOS swipe action
                    .swipeActions(edge: .trailing, allowsFullSwipe: true) {
                        Button(role: .destructive) {
                            deleteDisk(disk)
                        } label: {
                            Label("Delete", systemImage: "trash")
                        }
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding()
            }
            .toolbar {
                Button {
                    isShowingAddForm = true
                } label: {
                    Image(systemName: "plus")
                }
            }
            .sheet(isPresented: $isShowingAddForm) {
                AddDiskView { name, type in
                    // This closure executes when the user clicks 'Save'
                    let newDisk = Disk(name: name, type: type, flightNumbers: [])
                    disks.append(newDisk)
                    DiskStore.save(disks) // Persist the new disk
                }
            }
        }
        .navigationTitle("Stash")
        .onAppear {
            disks = DiskStore.load()
        }
    }
    
    private func deleteDisk(_ disk: Disk) {
        // 1. Find the index of the specific disk using its unique ID
        if let index = disks.firstIndex(where: { $0.id == disk.id }) {
            
            // 2. Remove it from the local @State array (this updates the UI instantly)
            disks.remove(at: index)
            
            // 3. Save the newly updated array to disk using your store helper
            DiskStore.save(disks)
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
            .animation(.linear(duration: 0.05), value: clampedValue)

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
