//
//  GeneralSettingsView.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import SwiftUI
import SumUpSDK

struct GeneralSettingsView: View {
    @AppStorage("businessName") private var businessName: String = "Five Points Cocktail Bar"
    @AppStorage("location") private var location: String = ""
    @AppStorage("profitMargin") private var profitMargin: Double = 0.0
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    @AppStorage("tableLayoutType") private var tableLayoutType: String = "Grid"
    private let currencyCodes = ["EUR", "USD", "GBP"]
    private let layoutTypes = ["Grid", "Custom"]

    @AppStorage("sumupAPIKey") private var sumupAPIKey: String = ""
    @AppStorage("defaultEmailBody") private var defaultEmailBody: String = "Thank you for your purchase. Please find your receipt attached."

    var body: some View {
        Form {
            Section(header: Text("Table View")) {
                Picker("Layout Style", selection: $tableLayoutType) {
                    ForEach(layoutTypes, id: \.self) {
                        Text($0)
                    }
                }
                .pickerStyle(SegmentedPickerStyle())
            }

            Section(header: Text("Business Details")) {
                TextField("Business Name", text: $businessName)
                TextField("Location", text: $location)
                TextField("Profit Margin", value: $profitMargin, format: .percent)
                    #if os(iOS)
                    .keyboardType(.decimalPad)
                    #endif
            }

            Section(header: Text("Hardware")) {
                NavigationLink(destination: PrinterSettingsView()) {
                    Text("Manage Printers")
                }
            }

            Section(header: Text("Tax Management")) {
                NavigationLink(destination: TaxBandManagementView()) {
                    Text("Manage Tax Bands")
                }
            }

            Section(header: Text("Regional")) {
                Picker("Currency", selection: $currencyCode) {
                    ForEach(currencyCodes, id: \.self) { code in
                        Text(code).tag(code)
                    }
                }
            }

            Section(header: Text("Email Settings")) {
                TextEditor(text: $defaultEmailBody)
                    .frame(height: 100)
            }

            Section(header: Text("SumUp API")) {
                TextField("API Key", text: $sumupAPIKey)

                Button("Login to SumUp") {
                    guard let windowScene = UIApplication.shared.connectedScenes.first as? UIWindowScene,
                          let rootViewController = windowScene.windows.first?.rootViewController else {
                        return
                    }
                    SumUpSDK.presentLogin(from: rootViewController, animated: true) { (success, error) in
                        if let error = error {
                            print("Error logging in to SumUp: \(error.localizedDescription)")
                        }
                    }
                }
            }
        }
        .navigationTitle("General Settings")
    }
}

struct GeneralSettingsView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            GeneralSettingsView()
        }
    }
}
