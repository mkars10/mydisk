//
//  MyDiskApp.swift
//  MyDisk
//
//  Created by Michael Karsten on 9/28/25.
//

import SwiftUI

@main
struct MyDiskApp: App {
    var body: some Scene {
        WindowGroup {
            TabView {
                ContentView()
                    .tabItem { Label("Finder", systemImage: "dot.radiowaves.left.and.right") }
                FinderStatsView()   // MyDiskKit test page (Finder/)
                    .tabItem { Label("Finder Stats", systemImage: "list.bullet.rectangle") }
            }
        }
    }
}
