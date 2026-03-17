import SwiftUI

struct MainView: View {
    @EnvironmentObject var posData: POSData
    @Binding var isDayStarted: Bool

    var body: some View {
        NavigationStack {
            ZStack {
                // Background gradient
                POSColors.backgroundGradient
                    .ignoresSafeArea()
                
                VStack(spacing: 40) {
                    // App Header
                    appHeader
                    
                    Spacer()
                    
                    // Main Navigation Cards
                    navigationCards
                    
                    Spacer()
                    
                    // App Info Footer
                    appFooter
                }
                .padding(.horizontal, 40)
                .padding(.vertical, 60)
            }
            .navigationTitle("")
            .navigationBarHidden(true)
        }
        .preferredColorScheme(.dark)
    }
    
    // MARK: - App Header
    private var appHeader: some View {
        VStack(spacing: 16) {
            // App Icon/Logo Placeholder
            RoundedRectangle(cornerRadius: POSRadius.large)
                .fill(POSColors.goldGradient)
                .frame(width: 80, height: 80)
                .overlay(
                    Image(systemName: "menucard")
                        .font(.system(size: 40, weight: .light))
                        .foregroundColor(POSColors.textPrimary)
                )
                .shadow(
                    color: POSShadows.elevated.color,
                    radius: POSShadows.elevated.radius,
                    x: POSShadows.elevated.x,
                    y: POSShadows.elevated.y
                )
            
            VStack(spacing: 8) {
                Text("Point of Sale")
                    .font(.largeTitle.weight(.bold))
                    .foregroundColor(POSColors.textPrimary)
                
                Text("Professional Restaurant Management")
                    .font(.title3.weight(.medium))
                    .foregroundColor(POSColors.textSecondary)
                    .multilineTextAlignment(.center)
            }
        }
    }
    
    // MARK: - Navigation Cards
    private var navigationCards: some View {
        VStack(spacing: 24) {
            // Start Day - Primary Action
            NavigationLink(destination: StartDayView(isDayStarted: $isDayStarted)) {
                HStack(spacing: 16) {
                    Image(systemName: "play.circle.fill")
                        .font(.title)
                        .foregroundColor(POSColors.textPrimary)
                    
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Start Day")
                            .font(.title2.weight(.semibold))
                            .foregroundColor(POSColors.textPrimary)
                        
                        Text("Begin daily operations")
                            .font(.subheadline)
                            .foregroundColor(POSColors.textPrimary.opacity(0.8))
                    }
                    
                    Spacer()
                    
                    Image(systemName: "chevron.right")
                        .font(.title3.weight(.medium))
                        .foregroundColor(POSColors.textPrimary.opacity(0.6))
                }
                .padding(24)
                .frame(maxWidth: .infinity)
                .background(
                    RoundedRectangle(cornerRadius: POSRadius.medium)
                        .fill(POSColors.goldPrimary)
                        .shadow(
                            color: POSShadows.card.color,
                            radius: POSShadows.card.radius,
                            x: POSShadows.card.x,
                            y: POSShadows.card.y
                        )
                )
            }
            .buttonStyle(PlainButtonStyle())
            
            HStack(spacing: 24) {
                // Data/Reports
                NavigationLink(destination: PastReportsView()) {
                    VStack(spacing: 12) {
                        Image(systemName: "chart.bar.fill")
                            .font(.title)
                            .foregroundColor(POSColors.goldPrimary)
                        
                        VStack(spacing: 4) {
                            Text("Analytics")
                                .font(.headline.weight(.semibold))
                                .foregroundColor(POSColors.textPrimary)
                            
                            Text("View reports & data")
                                .font(.caption)
                                .foregroundColor(POSColors.textSecondary)
                                .multilineTextAlignment(.center)
                        }
                    }
                    .padding(20)
                    .frame(maxWidth: .infinity)
                    .aspectRatio(1.2, contentMode: .fit)
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
                }
                .buttonStyle(PlainButtonStyle())
                
                // Settings
                NavigationLink(destination: SettingsView()) {
                    VStack(spacing: 12) {
                        Image(systemName: "gearshape.fill")
                            .font(.title)
                            .foregroundColor(POSColors.goldPrimary)
                        
                        VStack(spacing: 4) {
                            Text("Settings")
                                .font(.headline.weight(.semibold))
                                .foregroundColor(POSColors.textPrimary)
                            
                            Text("Configure system")
                                .font(.caption)
                                .foregroundColor(POSColors.textSecondary)
                                .multilineTextAlignment(.center)
                        }
                    }
                    .padding(20)
                    .frame(maxWidth: .infinity)
                    .aspectRatio(1.2, contentMode: .fit)
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
                }
                .buttonStyle(PlainButtonStyle())
            }
        }
        .frame(maxWidth: 600)
    }
    
    // MARK: - App Footer
    private var appFooter: some View {
        VStack(spacing: 8) {
            HStack(spacing: 16) {
                statusIndicator
                
                Spacer()
                
                if posData.hasOpenTables {
                    openTablesWarning
                }
            }
            
            Text("Professional POS System v1.0")
                .font(.caption)
                .foregroundColor(POSColors.textMuted)
        }
    }
    
    private var statusIndicator: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(POSColors.success)
                .frame(width: 8, height: 8)
            
            Text("System Ready")
                .font(.caption.weight(.medium))
                .foregroundColor(POSColors.textSecondary)
        }
    }
    
    private var openTablesWarning: some View {
        HStack(spacing: 8) {
            Image(systemName: "exclamationmark.triangle.fill")
                .font(.caption)
                .foregroundColor(POSColors.warning)
            
            Text("Open tables detected")
                .font(.caption.weight(.medium))
                .foregroundColor(POSColors.warning)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(
            RoundedRectangle(cornerRadius: POSRadius.small)
                .fill(POSColors.warning.opacity(0.1))
                .overlay(
                    RoundedRectangle(cornerRadius: POSRadius.small)
                        .stroke(POSColors.warning.opacity(0.3), lineWidth: 1)
                )
        )
    }
}

// MARK: - Preview
struct MainView_Previews: PreviewProvider {
    static var previews: some View {
        MainView(isDayStarted: .constant(false))
            .environmentObject(POSData())
            .preferredColorScheme(.dark)
    }
}
