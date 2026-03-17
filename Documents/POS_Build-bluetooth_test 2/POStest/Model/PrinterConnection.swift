import Foundation

protocol PrinterConnection {
    func send(data: Data, completion: @escaping (Bool) -> Void)
}
