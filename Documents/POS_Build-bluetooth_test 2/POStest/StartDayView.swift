//
//  StartDayView.swift
//  POStest
//

import SwiftUI

struct StartDayView: View {
    @EnvironmentObject var posData: POSData
    @Binding var isDayStarted: Bool

    @State private var selectedStaff: Set<UUID> = []
    @State private var existingDay: Day?
    @State private var showReopenAlert = false

    var body: some View {
        ScrollView {
            VStack(spacing: 24) {
                // Date header
                VStack(spacing: 4) {
                    Text(Date().formatted(.dateTime.weekday(.wide).day().month(.wide).year()))
                        .font(.title2.weight(.semibold))
                        .foregroundColor(POSColors.textPrimary)
                    Text(Date().formatted(.dateTime.hour().minute()))
                        .font(.headline)
                        .foregroundColor(POSColors.goldPrimary)
                }
                .padding(.top, 8)

                // Staff on shift
                VStack(spacing: 16) {
                    HStack {
                        Text("Who's on shift?")
                            .font(.headline.weight(.semibold))
                            .foregroundColor(POSColors.textPrimary)
                        Spacer()
                        if !selectedStaff.isEmpty {
                            Text("\(selectedStaff.count) selected")
                                .font(.caption.weight(.medium))
                                .foregroundColor(POSColors.goldPrimary)
                        }
                    }

                    if posData.employees.isEmpty {
                        VStack(spacing: 12) {
                            Image(systemName: "person.badge.plus")
                                .font(.system(size: 40))
                                .foregroundColor(POSColors.textMuted)
                            Text("No staff added yet")
                                .font(.subheadline)
                                .foregroundColor(POSColors.textMuted)
                            Text("Add employees in Settings to track staff costs.")
                                .font(.caption)
                                .foregroundColor(POSColors.textDisabled)
                                .multilineTextAlignment(.center)
                        }
                        .padding(20)
                    } else {
                        VStack(spacing: 10) {
                            ForEach(posData.employees) { employee in
                                staffRow(employee)
                            }
                        }
                    }
                }
                .padding(20)
                .background(
                    RoundedRectangle(cornerRadius: POSRadius.medium)
                        .fill(POSColors.backgroundMedium)
                )

                // Confirm button
                Button("Start Day") {
                    let names = posData.employees
                        .filter { selectedStaff.contains($0.id) }
                        .map(\.name)

                    if let day = posData.findDayForToday() {
                        existingDay = day
                        showReopenAlert = true
                    } else {
                        posData.startDay(staff: names)
                        isDayStarted = true
                    }
                }
                .buttonStyle(PrimaryButtonStyle())
            }
            .padding(20)
        }
        .background(POSColors.backgroundGradient)
        .navigationTitle("Start Day")
        .navigationBarTitleDisplayMode(.large)
        .preferredColorScheme(.dark)
        .alert("Today already has a report.", isPresented: $showReopenAlert, presenting: existingDay) { day in
            Button("Reopen") {
                posData.reopenDay(day)
                isDayStarted = true
            }
            Button("Start New") {
                let names = posData.employees
                    .filter { selectedStaff.contains($0.id) }
                    .map(\.name)
                posData.startDay(staff: names)
                isDayStarted = true
            }
            Button("Cancel", role: .cancel) {}
        } message: { _ in
            Text("You can reopen the existing session or start a new one.")
        }
    }

    private func staffRow(_ employee: Employee) -> some View {
        let isSelected = selectedStaff.contains(employee.id)
        return Button {
            if isSelected {
                selectedStaff.remove(employee.id)
            } else {
                selectedStaff.insert(employee.id)
            }
        } label: {
            HStack(spacing: 14) {
                ZStack {
                    Circle()
                        .fill(isSelected ? POSColors.goldPrimary : POSColors.backgroundLight.opacity(0.3))
                        .frame(width: 36, height: 36)
                    Text(String(employee.name.prefix(1)).uppercased())
                        .font(.headline.weight(.bold))
                        .foregroundColor(isSelected ? POSColors.backgroundDark : POSColors.textMuted)
                }

                Text(employee.name)
                    .font(.body.weight(.medium))
                    .foregroundColor(isSelected ? POSColors.textPrimary : POSColors.textSecondary)

                Spacer()

                Image(systemName: isSelected ? "checkmark.circle.fill" : "circle")
                    .font(.title3)
                    .foregroundColor(isSelected ? POSColors.goldPrimary : POSColors.textDisabled)
            }
            .padding(14)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.small)
                    .fill(isSelected
                          ? POSColors.goldPrimary.opacity(0.08)
                          : POSColors.backgroundLight.opacity(0.08))
                    .overlay(
                        RoundedRectangle(cornerRadius: POSRadius.small)
                            .strokeBorder(
                                isSelected ? POSColors.goldPrimary.opacity(0.4) : Color.clear,
                                lineWidth: 1
                            )
                    )
            )
        }
        .buttonStyle(.plain)
        .animation(.easeInOut(duration: 0.15), value: isSelected)
    }
}

struct StartDayView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            StartDayView(isDayStarted: .constant(false))
                .environmentObject(POSData())
        }
        .preferredColorScheme(.dark)
    }
}
