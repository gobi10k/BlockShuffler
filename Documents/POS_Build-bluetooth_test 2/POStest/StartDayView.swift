//
//  StartDayView.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import SwiftUI

struct StartDayView: View {
    @EnvironmentObject var posData: POSData
    @Binding var isDayStarted: Bool

    @State private var selectedStaff: [String] = []
    @State private var existingDay: Day?
    @State private var showReopenAlert = false

    var body: some View {
        VStack {
            Text(Date().formatted(date: .long, time: .shortened))
                .font(.title2)

            // Placeholder for weather
            Text("Weather: Sunny, 25°C")
                .font(.title2)
                .padding(.bottom)

            List(posData.employees) { employee in
                Button(action: {
                    if selectedStaff.contains(employee.name) {
                        selectedStaff.removeAll { $0 == employee.name }
                    } else {
                        selectedStaff.append(employee.name)
                    }
                }) {
                    HStack {
                        Text(employee.name)
                        Spacer()
                        if selectedStaff.contains(employee.name) {
                            Image(systemName: "checkmark.square.fill")
                        } else {
                            Image(systemName: "square")
                        }
                    }
                }
                .foregroundColor(.primary)
            }

            Button("Confirm") {
                if let day = posData.findDayForToday() {
                    existingDay = day
                    showReopenAlert = true
                } else {
                    posData.startDay(staff: selectedStaff)
                    isDayStarted = true
                }
            }
            .font(.title)
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.blue)
            .foregroundColor(.white)
            .cornerRadius(10)
            .padding()
        }
        .navigationTitle("Start Day")
        .alert("A report for today already exists.", isPresented: $showReopenAlert, presenting: existingDay) { day in
            Button("Reopen") {
                posData.reopenDay(day)
                isDayStarted = true
            }
            Button("Start New") {
                posData.startDay(staff: selectedStaff)
                isDayStarted = true
            }
            Button("Cancel", role: .cancel) { }
        }
    }
}

struct StartDayView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            StartDayView(isDayStarted: .constant(false))
                .environmentObject(POSData())
        }
    }
}
