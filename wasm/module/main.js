import * as readline from 'node:readline/promises';
import {stdin, stdout} from 'node:process';
import {BatchTranslator} from "./translator.js";

const rl = readline.createInterface({input: stdin, output: stdout});

const translator = new BatchTranslator();

for await (const line of rl) {
	const response = await translator.translate({
		from: "en",
		to: "es",
		text: line,
		html: false,
		qualityScores: false
	});

	console.log(response.target.text);
}

translator.delete();
