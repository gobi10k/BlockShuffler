import Foundation

enum VoidReason: String, CaseIterable, Codable, Identifiable {
    case mistake        = "Mistake / Wrong Item"
    case spillage       = "Spillage"
    case complimentary  = "Complimentary"
    case customerChange = "Customer Changed Mind"
    case quality        = "Quality Issue"
    case other          = "Other"

    var id: String { rawValue }

    var icon: String {
        switch self {
        case .mistake:        return "xmark.circle"
        case .spillage:       return "drop.triangle"
        case .complimentary:  return "gift"
        case .customerChange: return "arrow.uturn.backward"
        case .quality:        return "exclamationmark.triangle"
        case .other:          return "ellipsis.circle"
        }
    }
}
