//
//  LiveTableButtonStyle.swift
//  POStest
//
//  Created by Alec Gordon on 14/08/2025.
//
import SwiftUI
// MARK: - Weather Service for Location-Based Weather
import CoreLocation

class WeatherService: NSObject, ObservableObject, CLLocationManagerDelegate {
    @Published var temperature: String = "Loading..."
    @Published var condition: String = "Loading..."
    @Published var locationName: String = "Loading..."
    
    private let locationManager = CLLocationManager()
    private let apiKey = "YOUR_WEATHER_API_KEY" // Replace with actual API key
    
    override init() {
        super.init()
        locationManager.delegate = self
        locationManager.requestWhenInUseAuthorization()
    }
    
    func requestLocation() {
        locationManager.requestLocation()
    }
    
    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard let location = locations.first else { return }
        fetchWeather(for: location)
    }
    
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        print("Location error: \(error.localizedDescription)")
        // Fallback to default values
        DispatchQueue.main.async {
            self.temperature = "22°C"
            self.condition = "Partly Cloudy"
            self.locationName = "Current Location"
        }
    }
    
    private func fetchWeather(for location: CLLocation) {
        // This is a placeholder - implement actual weather API call
        // For now, providing more realistic default values
        DispatchQueue.main.async {
            self.temperature = "\(Int.random(in: 18...28))°C"
            self.condition = ["Sunny", "Partly Cloudy", "Cloudy", "Clear"].randomElement() ?? "Clear"
            self.locationName = "Current Location"
        }
    }
}

// MARK: - Updated Close Table with Tip Above Total
struct CloseTableView: View {
    @Binding var table: Table
    @State private var tipAmount: Double = 0
    @State private var customerEmail: String = ""
    @State private var paymentMethod: String = "Cash"
    @State private var showEmailSent: Bool = false
    
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"
    @Environment(\.dismiss) private var dismiss
    
    var totalWithTip: Double {
        table.total + tipAmount
    }
    
    var body: some View {
        VStack(spacing: 24) {
            // Header
            Text("Close Table \(table.id)")
                .font(.largeTitle.weight(.bold))
                .foregroundColor(POSColors.textPrimary)
            
            // Order Summary
            VStack(spacing: 16) {
                HStack {
                    Text("Subtotal:")
                    Spacer()
                    Text(table.subtotal.formatted(.currency(code: currencyCode)))
                }
                .foregroundColor(POSColors.textSecondary)
                
                HStack {
                    Text("Tax:")
                    Spacer()
                    Text(table.tax.formatted(.currency(code: currencyCode)))
                }
                .foregroundColor(POSColors.textSecondary)
                
                HStack {
                    Text("Order Total:")
                    Spacer()
                    Text(table.total.formatted(.currency(code: currencyCode)))
                }
                .font(.headline)
                .foregroundColor(POSColors.textPrimary)
                
                Divider()
                    .background(POSColors.goldSubtle.opacity(0.3))
                
                // Tip Input
                VStack(alignment: .leading, spacing: 8) {
                    Text("Tip Amount:")
                        .font(.headline)
                        .foregroundColor(POSColors.textPrimary)
                    
                    TextField("0.00", value: $tipAmount, format: .currency(code: currencyCode))
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .keyboardType(.decimalPad)
                }
                
                HStack {
                    Text("Total with Tip:")
                    Spacer()
                    Text(totalWithTip.formatted(.currency(code: currencyCode)))
                }
                .font(.title.weight(.bold))
                .foregroundColor(POSColors.goldPrimary)
            }
            .padding()
            .background(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .fill(POSColors.backgroundMedium)
                    .shadow(
                        color: POSShadows.card.color,
                        radius: POSShadows.card.radius,
                        x: POSShadows.card.x,
                        y: POSShadows.card.y
                    )
            )
            
            // Email Section
            VStack(alignment: .leading, spacing: 8) {
                Text("Customer Email (Optional):")
                    .font(.headline)
                    .foregroundColor(POSColors.textPrimary)
                
                HStack {
                    TextField("customer@example.com", text: $customerEmail)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .keyboardType(.emailAddress)
                    
                    Button("Send") {
                        sendReceipt()
                    }
                    .buttonStyle(SecondaryButtonStyle())
                    .disabled(customerEmail.isEmpty)
                }
                
                if showEmailSent {
                    HStack {
                        Image(systemName: "checkmark.circle.fill")
                            .foregroundColor(POSColors.success)
                        Text("Receipt sent successfully!")
                            .foregroundColor(POSColors.success)
                    }
                    .font(.caption)
                }
            }
            
            // Payment Method
            VStack(alignment: .leading, spacing: 8) {
                Text("Payment Method:")
                    .font(.headline)
                    .foregroundColor(POSColors.textPrimary)
                
                Picker("Payment Method", selection: $paymentMethod) {
                    Text("Cash").tag("Cash")
                    Text("Card").tag("Card")
                    Text("Mobile").tag("Mobile")
                }
                .pickerStyle(SegmentedPickerStyle())
            }
            
            Spacer()
            
            // Action Buttons
            VStack(spacing: 12) {
                Button("Complete Payment") {
                    // Close table logic here
                    dismiss()
                }
                .buttonStyle(PrimaryButtonStyle())
                
                Button("Cancel") {
                    dismiss()
                }
                .buttonStyle(SecondaryButtonStyle())
            }
        }
        .padding()
        .background(POSColors.backgroundGradient)
    }
    
    private func sendReceipt() {
        // Simulate sending email
        withAnimation {
            showEmailSent = true
            customerEmail = ""
        }
        
        DispatchQueue.main.asyncAfter(deadline: .now() + 3) {
            withAnimation {
                showEmailSent = false
            }
        }
    }
}
