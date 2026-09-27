import SwiftUI

struct AddDiskView: View {
    @Environment(\.dismiss) var dismiss
        
    @State private var name: String = ""
    @State private var type: String = "Driver"
    let diskTypes = ["Driver", "Mid", "Approach"]
    
    var onSave: (String, String) -> Void
    
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
                        onSave(name, type)
                        dismiss()
                    }
                    .disabled(name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
            }
        }
    }
}
