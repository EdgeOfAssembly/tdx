// Export function entry points as TSV for TDX (seg:off\tname or linear\tname).
//@category TDX

import java.io.FileWriter;
import java.io.IOException;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ExportRexSymbols extends GhidraScript {
	@Override
	public void run() throws Exception {
		String outPath = "rex_symbols.tsv";
		String[] args = getScriptArgs();
		if (args != null && args.length > 0 && args[0] != null && !args[0].isEmpty()) {
			outPath = args[0];
		}
		try (FileWriter w = new FileWriter(outPath)) {
			w.write("# TDX symbols from Ghidra: addr\\tname\n");
			FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
			while (it.hasNext()) {
				Function f = it.next();
				w.write(f.getEntryPoint().toString());
				w.write('\t');
				w.write(f.getName());
				w.write('\n');
			}
		} catch (IOException e) {
			printerr("ExportRexSymbols: " + e.getMessage());
			throw e;
		}
		println("ExportRexSymbols wrote " + outPath);
	}
}
