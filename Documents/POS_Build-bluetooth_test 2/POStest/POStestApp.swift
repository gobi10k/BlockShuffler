//  POStestApp.swift
import SwiftUI
import SumUpSDK

@main
struct POStestApp: App {
    @StateObject private var posData = POSData()
    @State private var isDayStarted = false

    init() {
        let apiKey = UserDefaults.standard.string(forKey: "sumupAPIKey") ?? ""
        SumUpSDK.setup(withAPIKey: apiKey)
    }

    var body: some Scene {
        WindowGroup {
            if isDayStarted {
                InteractionView(isDayStarted: $isDayStarted)
                    .environmentObject(posData)
                    .preferredColorScheme(.dark) // Add this line
            } else {
                MainView(isDayStarted: $isDayStarted)
                    .environmentObject(posData)
                    .preferredColorScheme(.dark) // Add this line
            }
        }
    }
}
