//
//  LiveTimerView.swift
//  POStest
//
//  Created by Alec Gordon on 14/08/2025.
//

import SwiftUI

// MARK: - Live Timer View
struct LiveTimerView: View {
    let startDate: Date
    @State private var timeElapsed: TimeInterval = 0
    @State private var timer: Timer?
    
    private static let formatter: DateComponentsFormatter = {
        let formatter = DateComponentsFormatter()
        formatter.allowedUnits = [.hour, .minute, .second]
        formatter.unitsStyle = .positional
        formatter.zeroFormattingBehavior = .pad
        return formatter
    }()
    
    var body: some View {
        Text(LiveTimerView.formatter.string(from: timeElapsed) ?? "00:00:00")
            .onAppear {
                startTimer()
            }
            .onDisappear {
                stopTimer()
            }
    }
    
    private func startTimer() {
        updateTime()
        timer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
            updateTime()
        }
    }
    
    private func stopTimer() {
        timer?.invalidate()
        timer = nil
    }
    
    private func updateTime() {
        timeElapsed = Date().timeIntervalSince(startDate)
    }
}

// MARK: - Updated Table Button Style with Live Timers
struct LiveTableButtonStyle: ButtonStyle {
    let table: Table
    let isSelected: Bool

    private func backgroundColor(for table: Table) -> Color {
        guard let openedAt = table.openedAt else {
            return POSColors.backgroundMedium
        }

        let duration = Date().timeIntervalSince(openedAt)
        let thirtyMinutes = 30.0 * 60.0
        let percentage = min(duration / thirtyMinutes, 1.0)

        // Sophisticated color transition from green to amber to red
        if percentage < 0.5 {
            // Green to amber transition
            let localPercentage = percentage * 2
            return Color(
                red: 0.2 + (localPercentage * 0.6),   // 0.2 to 0.8
                green: 0.8 - (localPercentage * 0.1), // 0.8 to 0.7
                blue: 0.2
            )
        } else {
            // Amber to red transition
            let localPercentage = (percentage - 0.5) * 2
            return Color(
                red: 0.8 + (localPercentage * 0.2),   // 0.8 to 1.0
                green: 0.7 - (localPercentage * 0.5), // 0.7 to 0.2
                blue: 0.2 - (localPercentage * 0.1)   // 0.2 to 0.1
            )
        }
    }

    func makeBody(configuration: Configuration) -> some View {
        VStack(spacing: 8) {
            configuration.label
                .font(.title2.weight(.medium))
                .foregroundColor(POSColors.textPrimary)

            if let openedAt = table.openedAt {
                LiveTimerView(startDate: openedAt)
                    .font(.caption.weight(.medium))
                    .foregroundColor(POSColors.textSecondary)
            }

            if let lastOrderItemAt = table.lastOrderItemAt {
                LiveTimerView(startDate: lastOrderItemAt)
                    .font(.caption2)
                    .foregroundColor(POSColors.textMuted)
            }
        }
        .frame(width: 100, height: 100)
        .background(
            RoundedRectangle(cornerRadius: POSRadius.medium)
                .fill(backgroundColor(for: table))
                .overlay(
                    RoundedRectangle(cornerRadius: POSRadius.medium)
                        .fill(configuration.isPressed ? POSColors.pressedOverlay : Color.clear)
                )
        )
        .overlay(
            RoundedRectangle(cornerRadius: POSRadius.medium)
                .stroke(
                    isSelected ? POSColors.goldPrimary : POSColors.goldSubtle.opacity(0.3),
                    lineWidth: isSelected ? 3 : 1
                )
        )
        .shadow(
            color: POSShadows.card.color,
            radius: configuration.isPressed ? POSShadows.pressed.radius : POSShadows.card.radius,
            x: POSShadows.card.x,
            y: configuration.isPressed ? POSShadows.pressed.y : POSShadows.card.y
        )
        .scaleEffect(configuration.isPressed ? 0.97 : 1.0)
        .animation(.easeOut(duration: 0.15), value: configuration.isPressed)
        .animation(.easeInOut(duration: 0.3), value: isSelected)
    }
}
