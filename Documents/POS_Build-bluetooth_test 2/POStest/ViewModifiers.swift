import SwiftUI

// MARK: - Table Button Style (Dark Sophisticated)
struct TableButtonStyle: ButtonStyle {
    let table: Table
    let isSelected: Bool
    let now: Date

    private static let formatter: DateComponentsFormatter = {
        let formatter = DateComponentsFormatter()
        formatter.allowedUnits = [.hour, .minute, .second]
        formatter.unitsStyle = .positional
        formatter.zeroFormattingBehavior = .pad
        return formatter
    }()

    private func backgroundColor(for table: Table) -> Color {
        guard let openedAt = table.openedAt else {
            return POSColors.backgroundMedium
        }

        let duration = now.timeIntervalSince(openedAt)
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
                let duration = now.timeIntervalSince(openedAt)
                Text(TableButtonStyle.formatter.string(from: duration) ?? "00:00:00")
                    .font(.caption.weight(.medium))
                    .foregroundColor(POSColors.textSecondary)
            }

            if let lastOrderItemAt = table.lastOrderItemAt {
                let duration = now.timeIntervalSince(lastOrderItemAt)
                Text(TableButtonStyle.formatter.string(from: duration) ?? "00:00:00")
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

// MARK: - Primary Button Style (Dark Sophisticated)
struct PrimaryButtonStyle: ButtonStyle {
    var isDestructive: Bool = false
    
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.title2.weight(.semibold))
            .foregroundColor(POSColors.textPrimary)
            .padding(.vertical, 16)
            .padding(.horizontal, 24)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .fill(
                        isDestructive ?
                        LinearGradient(colors: [POSColors.error, POSColors.error], startPoint: .top, endPoint: .bottom) :
                        POSColors.goldGradient
                    )
                    .overlay(
                        RoundedRectangle(cornerRadius: POSRadius.medium)
                            .fill(configuration.isPressed ? POSColors.pressedOverlay : Color.clear)
                    )
            )
            .shadow(
                color: POSShadows.card.color,
                radius: configuration.isPressed ? POSShadows.pressed.radius : POSShadows.card.radius,
                x: POSShadows.card.x,
                y: configuration.isPressed ? POSShadows.pressed.y : POSShadows.card.y
            )
            .scaleEffect(configuration.isPressed ? 0.98 : 1.0)
            .animation(.easeOut(duration: 0.15), value: configuration.isPressed)
    }
}

// MARK: - Secondary Button Style (Dark Sophisticated)
struct SecondaryButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.title2.weight(.medium))
            .foregroundColor(configuration.isPressed ? POSColors.textPrimary : POSColors.goldPrimary)
            .padding(.vertical, 16)
            .padding(.horizontal, 24)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .fill(configuration.isPressed ? POSColors.goldPrimary : Color.clear)
                    .overlay(
                        RoundedRectangle(cornerRadius: POSRadius.medium)
                            .stroke(POSColors.goldPrimary, lineWidth: 2)
                    )
            )
            .shadow(
                color: POSShadows.card.color.opacity(0.3),
                radius: configuration.isPressed ? POSShadows.pressed.radius : 4,
                x: 0,
                y: configuration.isPressed ? POSShadows.pressed.y : 2
            )
            .scaleEffect(configuration.isPressed ? 0.98 : 1.0)
            .animation(.easeOut(duration: 0.15), value: configuration.isPressed)
    }
}

// MARK: - Menu Item Card Style (Dark Sophisticated)
struct MenuItemCardStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .padding(16)
            .frame(width: 150, height: 120)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .fill(POSColors.backgroundMedium)
                    .overlay(
                        RoundedRectangle(cornerRadius: POSRadius.medium)
                            .fill(configuration.isPressed ? POSColors.selectionOverlay : POSColors.hoverOverlay.opacity(0))
                    )
            )
            .overlay(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .stroke(POSColors.goldSubtle.opacity(0.3), lineWidth: 1)
            )
            .shadow(
                color: POSShadows.card.color,
                radius: configuration.isPressed ? POSShadows.pressed.radius : POSShadows.card.radius,
                x: POSShadows.card.x,
                y: configuration.isPressed ? POSShadows.pressed.y : POSShadows.card.y
            )
            .scaleEffect(configuration.isPressed ? 0.97 : 1.0)
            .animation(.easeOut(duration: 0.15), value: configuration.isPressed)
    }
}

// MARK: - Card Background Modifier
struct CardBackgroundModifier: ViewModifier {
    let color: Color
    
    init(color: Color = POSColors.backgroundMedium) {
        self.color = color
    }
    
    func body(content: Content) -> some View {
        content
            .background(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .fill(color)
                    .shadow(
                        color: POSShadows.card.color,
                        radius: POSShadows.card.radius,
                        x: POSShadows.card.x,
                        y: POSShadows.card.y
                    )
            )
    }
}

// MARK: - View Extensions
extension View {
    func cardBackground(_ color: Color = POSColors.backgroundMedium) -> some View {
        self.modifier(CardBackgroundModifier(color: color))
    }
}
