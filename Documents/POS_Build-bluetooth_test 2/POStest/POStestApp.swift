//  POStestApp.swift
import SwiftUI
import SumUpSDK

@main
struct POStestApp: App {
    @StateObject private var posData = POSData()
    @State private var isDayStarted = false
    @AppStorage("sumupAPIKey") private var sumupAPIKey: String = ""

    init() {
        let apiKey = UserDefaults.standard.string(forKey: "sumupAPIKey") ?? ""
        if !apiKey.isEmpty {
            SumUpSDK.setup(withAPIKey: apiKey)
        }
    }

    var body: some Scene {
        WindowGroup {
            Group {
                if isDayStarted {
                    InteractionView(isDayStarted: $isDayStarted)
                        .environmentObject(posData)
                } else {
                    MainView(isDayStarted: $isDayStarted)
                        .environmentObject(posData)
                }
            }
            .preferredColorScheme(.dark)
            .onChange(of: sumupAPIKey) { newKey in
                SumUpAPIManager.shared.setup(withAPIKey: newKey)
            }
        }
    }
}
