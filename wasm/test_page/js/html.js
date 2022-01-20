/**
 * HTML string literal tag. Example usage:
 *
 *	const el = html`<li>
 *		<label>
 *			<input type="radio" name="opt" value=${value}>
 *			Click here to choose ${label}
 *		</label>
 *	</li>`
 *
 * el will then be a DOMNode which you can append.
 */
const {html, htmlFragment} = (function() {
	/**
	 * Mismatch is thrown by the functions `peek`, `consume`, `one`, etc and
	 * will cause `any` or `one` to backtrack and try another option.
	 */
	class Mismatch extends Error {
		//
	}

	class TokenStream {
		constructor(tokens, pos = 0) {
			this.tokens = tokens;
			this.pos = pos;
		}

		peek() {
			if (this.pos >= this.tokens.length)
				throw new Mismatch('Out of tokens');

			return this.tokens[this.pos];
		}

		expect(literal) {
			if (literal instanceof RegExp)
				return this.peek().toString().match(literal);
			else
				return this.peek() == literal;
		}

		/**
		 * Consumes a token. If the optional `literal` argument is provided it
		 * will only consume that exact token, and fail otherwise.
		 */
		consume(literal) {
			if (this.pos >= this.tokens.length)
				throw new Mismatch('Out of tokens');

			if (literal && !this.expect(literal))
				throw new Mismatch(`Expected '${literal}' but found '${this.tokens[this.pos]}' instead`);

			return this.tokens[this.pos++];
		}

		/**
		 * Returns the first of the possible parses that don't fail. In this 
		 * scenario it returns a pair with the option that matched and its
		 * return value. If there is no match it will return `undefined`.
		 */
		any(options) {
			for (let option of options) {
				const pos = this.pos;

				try {
					return [option, option(this)];
				} catch(Mismatch) {
					this.pos = pos;
				}
			}

			return undefined;
		}

		/**
		 * Same as any, but will cause a Mismatch exception when it fails to
		 * parse one instead of returning undefined. I.e. this models a
		 * mandatory instead of optional match.
		 */
		one(options) {
			const out = this.any(options);
			
			if (out === undefined)
				throw new Mismatch('None of the options matched');

			return out;
		}
	}

	function consumeFragment(tokens) {
		const fragment = document.createDocumentFragment();

		for (let child; child = tokens.any([consumeElement, consumeEntity, consumeTextNode]);)
			fragment.appendChild(child[1])
		
		return fragment;
	}

	function consumeElement(tokens) {
		// Shortcut when you try to insert an actual HTML element.
		if (tokens.peek() instanceof HTMLElement)
			return tokens.consume();

		let el = consumeOpenTag(tokens);
		
		if (!['INPUT', 'IMG', 'BR', 'HR'].includes(el.nodeName)) {
			loop: {
				for (let child; child = tokens.any([consumeCloseTag, consumeElement, consumeEntity, consumeTextNode]);) {
					switch (child[0]) {
						case consumeElement:
						case consumeEntity:
						case consumeTextNode:
							el.appendChild(child[1]);
							break;

						case consumeCloseTag:
							if (child[1].toLowerCase() != el.nodeName.toLowerCase())
								console.warn(`Closing <${el.nodeName}> with </${child[1]}>?`);
							break loop;
					}
				}
			}
		}

		return el;
	}

	function consumeTextNode(tokens) {
		const text = tokens.consume();

		// Shortcut when you try to insert a DOM text node
		if (text instanceof Node)
			return text;
		
		return document.createTextNode(text);
	}

	function consumeEntity(tokens) {
		let entity = String(tokens.consume('&'));
		while (entity[entity.length - 1] != ';')
			entity += String(tokens.consume());
		const wrapper = document.createElement('div');
		wrapper.innerHTML = entity;
		return wrapper.firstChild;
	}

	function consumeWhitespace(tokens) {
		return tokens.consume(/^\s+$/);
	}

	function consumeText(tokens) {
		return tokens.consume();
	}

	function consumeQuotedString(tokens) {
		tokens.consume('"')

		let value = '';
		while (!tokens.expect('"'))
			value += String(tokens.consume());
		
		tokens.consume('"')
		return value;
	}

	function consumeAttribute(tokens) {
		let name = tokens.consume(/[a-zA-Z0-9\-]+/);

		if (tokens.expect('=')) {
			tokens.consume('=');
			let value = tokens.one([consumeQuotedString, consumeText]);
			return [name, value[1]];
		} else {
			return [name, true];
		}
	}

	function consumeOpenTag(tokens) {
		tokens.consume('<');
		const el = document.createElement(tokens.consume());
		for (let attr; attr = tokens.any([consumeAttribute, consumeWhitespace]);) {
			if (attr[0] == consumeAttribute) {
				const [key, value] = attr[1];

				// If it is a property, set it through Javascript (for event
				// handlers etc. this is the only way to go)
				if (key in el)
					el[key] = value;
				else
					el.setAttribute(key, value);
			}
		}
		tokens.consume('>');
		return el;
	}

	function consumeCloseTag(tokens) {
		tokens.consume('</');
		const name = tokens.consume();
		tokens.consume('>');
		return name;
	}

	function flatten(arr) {
		return Array.isArray(arr) ? arr : [arr];
	}

	function lexer(literals, vars, options = {}) {
		const tokens = Array.from(literals).reduce((tokens, literal, i) => {
			if (options.trim) {
				if (i == 0)
					literal = literal.trimStart();

				if (i == literals.length - 1)
					literal = literal.trimEnd();
			}

			return tokens
				.concat(
					literal.split(/(<\/?|>|"|=|&|;|\s)/g)
					.filter(token => token.length > 0)
				)
				.concat(i < vars.length ? flatten(vars[i]) : []);
		}, []);

		return new TokenStream(tokens);
	}

	function html(literals, ...vars) {
		return consumeElement(lexer(literals, vars, {trim:true}))
	}

	function htmlFragment(literals, ...vars) {
		return consumeFragment(lexer(literals, vars));
	}
	
	return {html, htmlFragment};
})();
