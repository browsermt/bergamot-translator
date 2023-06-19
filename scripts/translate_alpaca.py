#!/usr/bin/env python3
from bergamot.translator import Translator
from typing import List, Dict
import json
import argparse

def read_json(filename: str) -> List[Dict[str,str]]:
    lines: List[str] = []
    with open(filename, 'r', encoding='utf-8') as myfile:
        parsed = json.load(myfile)
        for item in parsed:
            for _, txt in item.items():
                lines.append(txt)
        return lines
    
def write_json(lines: List[str], outputname: str) -> None:
    outlist = []
    with open(outputname, 'w', encoding='utf-8') as writefile:
        state = 0
        curr_dict = {}
        for line in lines:
            if state == 0:
                curr_dict['instruction'] = line
                state = 1
                continue
            if state == 1:
                curr_dict['input'] = line
                state = 2
                continue
            if state == 2:
                curr_dict['output'] = line
                outlist.append(curr_dict)
                curr_dict = {}
                state = 0
                continue
        json.dump(outlist, writefile, ensure_ascii=False, indent = 4)

def main():
    parser = argparse.ArgumentParser(description="Translate alpaca form")
    parser.add_argument("--config", '-c', required=True, type=str, help='Translation Model YML configuration input.')
    parser.add_argument("--input", '-i', required=True, type=str, help="Input json alpaca training")
    parser.add_argument("--output", '-o', required=True, type=str, help="Output json alpaca training")
    parser.add_argument("--num-workers", '-n', type=int, default=1, help='Number of CPU workers.')
    args = parser.parse_args()
    
    translator = Translator(args.config, args.num_workers, 50) # Cache last argument
    lines = read_json(args.input)
    translations = translator.translate(lines)
    write_json(translations, args.output)

if __name__ == '__main__':
    main()
