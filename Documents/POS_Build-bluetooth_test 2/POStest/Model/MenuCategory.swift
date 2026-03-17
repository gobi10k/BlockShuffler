import SwiftUI

enum MenuCategory: String, CaseIterable, Codable, Identifiable {
    case cocktails = "Cocktails"
    case beer = "Beer & Cider"
    case wine = "Wine"
    case spirits = "Spirits"
    case softDrinks = "Soft Drinks"
    case hotDrinks = "Hot Drinks"
    case food = "Food"
    case other = "Other"

    var id: String { rawValue }

    var icon: String {
        switch self {
        case .cocktails:  return "wineglass"
        case .beer:       return "mug.fill"
        case .wine:       return "wineglass.fill"
        case .spirits:    return "drop.fill"
        case .softDrinks: return "bubbles.and.sparkles"
        case .hotDrinks:  return "cup.and.saucer.fill"
        case .food:       return "fork.knife"
        case .other:      return "ellipsis.circle"
        }
    }

    var color: Color {
        switch self {
        case .cocktails:  return Color(hex: "#E879F9")
        case .beer:       return Color(hex: "#FBBF24")
        case .wine:       return Color(hex: "#FB7185")
        case .spirits:    return Color(hex: "#60A5FA")
        case .softDrinks: return Color(hex: "#34D399")
        case .hotDrinks:  return Color(hex: "#F97316")
        case .food:       return Color(hex: "#A78BFA")
        case .other:      return Color(hex: "#94A3B8")
        }
    }
}
