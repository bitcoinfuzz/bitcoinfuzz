import { analyzeMiniscript } from "@bitcoinerlab/miniscript";

(globalThis as any).bitcoinerlab = {
    bitcoinerlabMiniscriptMiniscriptParse,
};

function isStackOverflowError(error: unknown): boolean {
    if (
        error instanceof Error &&
        error.name === "InternalError" &&
        error.message.includes("stack overflow")
    ) {
        return true;
    }
    return false;
}

function bitcoinerlabMiniscriptMiniscriptParse(input: ArrayBuffer): boolean {
    try {
        const bytes = new Uint8Array(input);
        let str = "";
        for (let i = 0; i < bytes.length; i++) {
            str += String.fromCharCode(bytes[i] as number);
        }
        // Bitcoin Core's miniscript parser is strict about whitespace;
        // bitcoinerlab trims its input, which causes false positives on
        // inputs like "\n0". Reject any whitespace to match Core.
        if (/\s/.test(str)) {
            return false;
        }
        const analysis = analyzeMiniscript(str);
        return analysis.issane;
    } catch (error) {
        if (isStackOverflowError(error)) {
            throw error;
        }
        return false;
    }
}
