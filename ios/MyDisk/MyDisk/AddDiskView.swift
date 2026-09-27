import SwiftUI

struct AddDiskView: View {
    @Environment(\.dismiss) var dismiss
        
    @State private var name: String = ""
    @State private var type: String = "Driver"
    let diskTypes = ["Driver", "Mid", "Approach"]
    @State private var speed: Int = 0
    @State private var glide: Int = 0
    @State private var turn: Int = 0
    @State private var fade: Int = 0
    
    var onSave: (String, String, Int, Int, Int, Int) -> Void
    
    var body: some View {
        NavigationStack {
            Form {
                Section(header: Text("Disk Details")) {
                    // Text input field for Name
                    TextField("Name", text: $name)
                        .autocorrectionDisabled()
                    
                    // Dropdown menu picker for Type
                    Picker("Type", selection: $type) {
                        ForEach(diskTypes, id: \.self) { type in
                            Text(type).tag(type)
                        }
                    }
                    
                    TextField("Speed", value: $speed, format: .number)
                        .keyboardType(.numberPad)
                    
                    TextField("Glide", value: $glide, format: .number)
                        .keyboardType(.numberPad)
                    
                    TextField("Turn", value: $turn, format: .number)
                        .keyboardType(.numberPad)
                    
                    TextField("Fade", value: $fade, format: .number)
                        .keyboardType(.numberPad)
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
