import SwiftUI

struct StashView: View {
    @State private var disks: [Disk] = []
    @State private var isShowingAddForm = false

    var body: some View {
        VStack(spacing: 16) {
            List {
                HStack {
                    Text("Disk Name").bold().frame(maxWidth: .infinity, alignment: .leading)
                    Text("Type").bold().frame(maxWidth: .infinity, alignment: .leading)
                    Text("Flight Numbers").bold().frame(maxWidth: .infinity, alignment: .leading)
                }
                .listRowBackground(Color.clear)
                
                ForEach(disks) { disk in
                    HStack {
                        Text(disk.name).frame(maxWidth: .infinity, alignment: .leading)
                        Text(disk.type).frame(maxWidth: .infinity, alignment: .leading)
                        Text([disk.speed, disk.glide, disk.turn, disk.fade].map(String.init).joined(separator: ", ")).frame(maxWidth: .infinity, alignment: .leading)
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
                AddDiskView { name, type, speed, glide, turn, fade in
                    let newDisk = Disk(name: name, type: type, speed: speed, glide: glide, turn: turn, fade: fade)
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
