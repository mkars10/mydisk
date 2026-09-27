import Foundation

// 1. Define the custom object shape
struct Disk: Codable, Identifiable {
    var id = UUID()
    var name: String
    var type: String
    var speed: Int
    var glide: Int
    var turn: Int
    var fade: Int
}

// 2. Wrap your persistence functions in a clean, reusable helper
struct DiskStore {
    static let fileURL = FileManager.default
        .urls(for: .documentDirectory, in: .userDomainMask)[0]
        .appendingPathComponent("disks.json")
    
    static let mockData: [Disk] = [
        Disk(name: "The Beast", type: "Distance Driver", speed: 7, glide: 2, turn: 0, fade: 1),
        Disk(name: "Leapord", type: "Midrange", speed: 4, glide: 3, turn: 0, fade: 1),
        Disk(name: "Kitten", type: "Putter", speed: 2, glide: 0, turn: 0, fade: 0)
    ]

    static func save(_ disks: [Disk]) {
        do {
            let data = try JSONEncoder().encode(disks)
            try data.write(to: fileURL, options: [.atomic, .completeFileProtection])
        } catch {
            print("Failed to save disks: \(error.localizedDescription)")
        }
    }
    
    static func load() -> [Disk] {
        if let data = try? Data(contentsOf: fileURL),
           let savedDisks = try? JSONDecoder().decode([Disk].self, from: data) {
            return savedDisks
        }
        
        // If no file exists, save the mock data to disk and return it
        print("First launch detected. Pre-populating with mock data.")
        save(mockData)
        return mockData
    }
}
