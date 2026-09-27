import SwiftUI

struct AddDiskView: View {
    @Environment(\.dismiss) var dismiss
        
    @State private var name: String = ""
    @State private var type: String = "Midrange"
    let diskTypes = ["Putter", "Midrange", "Fairway Driver", "Distance Driver"]
    @State private var speed: Int = 0
    @State private var glide: Int = 0
    @State private var turn: Int = 0
    @State private var fade: Int = 0
    
    var onSave: (String, String, Int, Int, Int, Int) -> Void
    
    var body: some View {
        NavigationStack {
            Form {
                Section(header: Text("Disk Details")) {
                    TextField("Name", text: $name)
                        .autocorrectionDisabled()
                    
                    Picker("Type", selection: $type) {
                        ForEach(diskTypes, id: \.self) { type in
                            Text(type).tag(type)
                        }
                    }
                    
                    Stepper("Speed: \(speed)", value: $speed, in: 1...14)
                    
                    Stepper("Glide: \(glide)", value: $glide, in: 1...7)
                    
                    Stepper("Turn: \(turn)", value: $turn, in: -5...1)
                    
                    Stepper("Fade: \(fade)", value: $fade, in: 0...5)
                }
            }
            .navigationTitle("Add New Disk")
            .toolbar {
                // Cancel button
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { dismiss() }
                }
                
                // Save button (disabled if name is empty)
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        onSave(name, type, speed, glide, turn, fade)
                        dismiss()
                    }
                    .disabled(name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
        }
    }
}
