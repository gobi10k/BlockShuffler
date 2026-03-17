//
//  GeneralSettingsView.swift
//  POStest
//

import SwiftUI
import SumUpSDK

struct GeneralSettingsView: View {
    @AppStorage("businessName")      private var businessName: String  = "Five Points Cocktail Bar"
    @AppStorage("location")          private var location: String      = ""
    @AppStorage("profitMargin")      private var profitMargin: Double  = 0.0
    @AppStorage("currencyCode")      private var currencyCode: String  = "EUR"
    @AppStorage("tableLayoutType")   private var tableLayoutType: String = "Grid"
    @AppStorage("sumupAPIKey")       private var sumupAPIKey: String   = ""
    @AppStorage("defaultEmailBody")  private var defaultEmailBody: String = "Thank you for your purchase. Please find your receipt attached."

    private let currencyCodes = ["EUR", "USD", "GBP"]
    private let layoutTypes   = ["Grid", "Custom"]

    @State private var isSumUpLoggedIn = false

    var body: some View {
        Form {
            Section(header: Text("Table View")) {
                Picker("Layout Style", selection: $tableLayoutType) {
                    ForEach(layoutTypes, id: \.self) { Text($0) }
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

            Section(header: Text("SumUp")) {
                TextField("API Key", text: $sumupAPIKey)
                    .autocapitalization(.none)
                    .onChange(of: sumupAPIKey) { newKey in
                        SumUpAPIManager.shared.setup(withAPIKey: newKey)
                        isSumUpLoggedIn = SumUpAPIManager.shared.isLoggedIn
                    }

                HStack {
                    Button("Login to SumUp") {
                        SumUpAPIManager.shared.presentLogin { success in
                            isSumUpLoggedIn = success
                        }
                    }
                    Spacer()
                    HStack(spacing: 6) {
                        Circle()
                            .fill(isSumUpLoggedIn ? Color.green : Color.red)
                            .frame(width: 10, height: 10)
                        Text(isSumUpLoggedIn ? "Logged in" : "Not logged in")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
            }
        }
        .navigationTitle("General Settings")
        .onAppear {
            isSumUpLoggedIn = SumUpAPIManager.shared.isLoggedIn
        }
    }
}

struct GeneralSettingsView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            GeneralSettingsView()
        }
    }
}
