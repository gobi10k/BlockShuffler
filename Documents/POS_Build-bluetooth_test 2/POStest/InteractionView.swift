import SwiftUI

struct InteractionView: View {
    @EnvironmentObject var posData: POSData
    @Binding var isDayStarted: Bool

    enum AppView {
        case tables, reports
    }

    @State private var selectedView: AppView = .tables

    var body: some View {
        VStack(spacing: 0) {
            // Main Content
            ZStack {
                if selectedView == .tables {
                    NavigationStack {
                        TableView()
                    }
                } else {
                    NavigationStack {
                        DailyReportView()
                    }
                }
            }

            // Custom Footer with Dark Sophisticated Design
            customFooter
        }
        .background(POSColors.backgroundDark)
        .ignoresSafeArea(.keyboard, edges: .bottom)
        .preferredColorScheme(.dark)
    }
    
    // MARK: - Custom Footer
    private var customFooter: some View {
        HStack(spacing: 0) {
            // Tables Tab
            tabButton(
                icon: "table.furniture",
                title: "Tables",
                isSelected: selectedView == .tables,
                action: { selectedView = .tables }
            )
            
            // Reports Tab
            tabButton(
                icon: "doc.text.magnifyingglass",
                title: "Reports",
                isSelected: selectedView == .reports,
                action: { selectedView = .reports }
            )
            
            // End Day Tab
            endDayButton
        }
        .padding(.top, 12)
        .padding(.bottom, 8)
        .background(
            Rectangle()
                .fill(POSColors.backgroundMedium)
                .shadow(color: POSShadows.card.color, radius: 8, x: 0, y: -4)
        )
    }
    
    private func tabButton(icon: String, title: String, isSelected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            VStack(spacing: 6) {
                Image(systemName: icon)
                    .font(.title2.weight(isSelected ? .semibold : .medium))
                    .foregroundColor(isSelected ? POSColors.goldPrimary : POSColors.textMuted)
                
                Text(title)
                    .font(.caption.weight(isSelected ? .semibold : .medium))
                    .foregroundColor(isSelected ? POSColors.goldPrimary : POSColors.textMuted)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 8)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.small)
                    .fill(isSelected ? POSColors.goldPrimary.opacity(0.1) : Color.clear)
            )
        }
        .animation(.easeInOut(duration: 0.2), value: isSelected)
    }
    
    private var endDayButton: some View {
        Button(action: {
            posData.endDay()
            isDayStarted = false
        }) {
            VStack(spacing: 6) {
                Image(systemName: posData.hasOpenTables ? "xmark.circle" : "power.circle")
                    .font(.title2.weight(.medium))
                    .foregroundColor(posData.hasOpenTables ? POSColors.textDisabled : POSColors.error)
                
                Text("End Day")
                    .font(.caption.weight(.medium))
                    .foregroundColor(posData.hasOpenTables ? POSColors.textDisabled : POSColors.error)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 8)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.small)
                    .fill(posData.hasOpenTables ? Color.clear : POSColors.error.opacity(0.1))
            )
        }
        .disabled(posData.hasOpenTables)
        .opacity(posData.hasOpenTables ? 0.5 : 1.0)
        .animation(.easeInOut(duration: 0.2), value: posData.hasOpenTables)
    }
}

struct InteractionView_Previews: PreviewProvider {
    static var previews: some View {
        InteractionView(isDayStarted: .constant(true))
            .environmentObject(POSData())
            .preferredColorScheme(.dark)
    }
}
