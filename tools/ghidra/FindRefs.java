// Prints every reference to a string (or an address) and the decompiled code of the referencing functions.
// Arguments: <mode: string|prefix|address|function> <string or hexadecimal address> <output file>
// Function mode only decompiles the function starting at the given address (creating it if Ghidra has not yet done so).
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class FindRefs extends GhidraScript {
  private DecompInterface decompiler;
  private final Set<Function> printed = new HashSet<>();

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    String mode = args[0];
    decompiler = new DecompInterface();
    decompiler.openProgram(currentProgram);
    try (PrintWriter out = new PrintWriter(new FileWriter(args[2]))) {
      if (mode.equals("function")) {
        Address entry = toAddr(Long.parseUnsignedLong(args[1].replace("0x", ""), 16));
        Function function = getFunctionAt(entry);
        if (function == null) function = createFunction(entry, null);
        printFunction(out, function);
        return;
      }
      for (Address target : targets(mode, args[1])) {
        out.println("target " + target);
        for (Reference ref : getReferencesTo(target)) {
          out.println("  ref from " + ref.getFromAddress() + " (" + ref.getReferenceType() + ")");
          printFunction(out, getFunctionContaining(ref.getFromAddress()));
          // Reference coming from data (for example a registration table): its referrers are needed too.
          for (Reference second : getReferencesTo(ref.getFromAddress())) {
            out.println("    data ref from " + second.getFromAddress());
            printFunction(out, getFunctionContaining(second.getFromAddress()));
          }
        }
      }
    }
  }

  private List<Address> targets(String mode, String value) throws Exception {
    List<Address> result = new ArrayList<>();
    if (mode.equals("address")) {
      result.add(toAddr(Long.parseUnsignedLong(value.replace("0x", ""), 16)));
      return result;
    }
    Memory memory = currentProgram.getMemory();
    byte[] needle = mode.equals("prefix") ? value.getBytes("US-ASCII") : (value + "\0").getBytes("US-ASCII");
    Address at = memory.getMinAddress();
    while ((at = memory.findBytes(at, needle, null, true, monitor)) != null) {
      // In string mode only an exact match counts, one preceded by a zero byte.
      if (mode.equals("prefix") || memory.getByte(at.subtract(1)) == 0) result.add(at);
      at = at.add(1);
    }
    return result;
  }

  private void printFunction(PrintWriter out, Function function) {
    if (function == null || !printed.add(function)) return;
    DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
    out.println("// ==== " + function.getName() + " @ " + function.getEntryPoint());
    out.println(result.decompileCompleted() ? result.getDecompiledFunction().getC() : "// decompile failed");
  }
}
