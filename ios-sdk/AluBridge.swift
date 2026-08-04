import Foundation

@_silgen_name("_process_image")
func _process_image(_ path: UnsafePointer<CChar>!) -> Int32

@_silgen_name("_process_packet")
func _process_packet(_ raw_bytes: UnsafePointer<Int8>!, _ length: Int32) -> Int32

/// AluBridge provides an idiomatic Swift interface over the native Alu LLVM/C engine.
public class AluBridge {
    public static let shared = AluBridge()
    
    public init() {}
    
    /// Scans a file path for threats using the Alu backend
    public func scanFile(filePath: String) -> Bool {
        return filePath.withCString { pathPtr in
            // Return true if Alu threat detected (1 = threat, 0 = safe)
            return _process_image(pathPtr) == 1
        }
    }
    
    /// Inspects raw network packet streams for threats using the Alu Network backend
    public func inspectPacket(rawBytes: Data) -> Bool {
        return rawBytes.withUnsafeBytes { buffer in
            guard let ptr = buffer.bindMemory(to: Int8.self).baseAddress else { return false }
            // process_packet returns 1 if threat detected (drop), 0 if safe (allow)
            return _process_packet(ptr, Int32(rawBytes.count)) == 1
        }
    }
}
