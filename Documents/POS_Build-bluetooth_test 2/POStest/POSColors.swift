//
//  POSColors.swift
//  POStest
//
//  Created by Alec Gordon on 14/08/2025.
//

import SwiftUI

struct POSColors {
    // MARK: - Background Colors
    static let backgroundDark = Color(hex: "#1A1D23")        // Deep charcoal with blue undertones
    static let backgroundMedium = Color(hex: "#2D3748")      // Charcoal gray for cards/panels
    static let backgroundLight = Color(hex: "#4A5568")       // Lighter gray for hover states
    static let surface = Color(hex: "#1E2329")               // Slightly lighter than background for elevated elements
    
    // MARK: - Gold Accent Colors
    static let goldPrimary = Color(hex: "#F6AD55")           // Warm gold for primary actions, highlights
    static let goldSecondary = Color(hex: "#ED8936")         // Deeper gold for pressed states
    static let goldSubtle = Color(hex: "#FBD38D")            // Light gold for subtle accents, borders
    
    // MARK: - Text Hierarchy
    static let textPrimary = Color.white                     // Pure white for headings, important info
    static let textSecondary = Color(hex: "#E2E8F0")         // Light gray for body text, descriptions
    static let textMuted = Color(hex: "#A0AEC0")             // Medium gray for secondary info, timestamps
    static let textDisabled = Color(hex: "#718096")          // Dark gray for disabled states
    
    // MARK: - Status Colors
    static let success = Color(hex: "#48BB78")               // Muted green for confirmations
    static let warning = Color(hex: "#ED8936")               // Orange for alerts, low stock
    static let error = Color(hex: "#F56565")                 // Soft red for errors, deletions
    static let info = Color(hex: "#4299E1")                  // Blue for informational messages
    
    // MARK: - Interactive States
    static let hoverOverlay = Color.white.opacity(0.1)      // Subtle white overlay for hover
    static let pressedOverlay = Color.black.opacity(0.2)    // Subtle dark overlay for pressed
    static let selectionOverlay = goldPrimary.opacity(0.2)  // Gold overlay for selections
    
    // MARK: - Gradients
    static let backgroundGradient = LinearGradient(
        colors: [backgroundDark, backgroundMedium],
        startPoint: .topLeading,
        endPoint: .bottomTrailing
    )
    
    static let goldGradient = LinearGradient(
        colors: [goldPrimary, goldSecondary],
        startPoint: .topLeading,
        endPoint: .bottomTrailing
    )
    
    static let overlayGradient = LinearGradient(
        colors: [Color.clear, backgroundDark.opacity(0.8)],
        startPoint: .top,
        endPoint: .bottom
    )
}

// MARK: - Color Extension for Hex Support
extension Color {
    init(hex: String) {
        let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        var int: UInt64 = 0
        Scanner(string: hex).scanHexInt64(&int)
        let a, r, g, b: UInt64
        switch hex.count {
        case 3: // RGB (12-bit)
            (a, r, g, b) = (255, (int >> 8) * 17, (int >> 4 & 0xF) * 17, (int & 0xF) * 17)
        case 6: // RGB (24-bit)
            (a, r, g, b) = (255, int >> 16, int >> 8 & 0xFF, int & 0xFF)
        case 8: // ARGB (32-bit)
            (a, r, g, b) = (int >> 24, int >> 16 & 0xFF, int >> 8 & 0xFF, int & 0xFF)
        default:
            (a, r, g, b) = (1, 1, 1, 0)
        }

        self.init(
            .sRGB,
            red: Double(r) / 255,
            green: Double(g) / 255,
            blue:  Double(b) / 255,
            opacity: Double(a) / 255
        )
    }
}

// MARK: - Shadow Styles
struct POSShadows {
    static let card = (color: Color.black.opacity(0.4), radius: CGFloat(6), x: CGFloat(0), y: CGFloat(4))
    static let elevated = (color: Color.black.opacity(0.5), radius: CGFloat(12), x: CGFloat(0), y: CGFloat(8))
    static let pressed = (color: Color.black.opacity(0.3), radius: CGFloat(3), x: CGFloat(0), y: CGFloat(2))
}

// MARK: - Corner Radius Constants
struct POSRadius {
    static let small: CGFloat = 6          // buttons, inputs
    static let medium: CGFloat = 12        // cards, panels
    static let large: CGFloat = 16         // large containers
    static let circular: CGFloat = 50      // circular elements percentage equivalent
}
