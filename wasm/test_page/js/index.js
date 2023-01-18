import {LatencyOptimisedTranslator, TranslatorBacking, CancelledError, SupersededError} from '../node_modules/@browsermt/bergamot-translator/translator.js';

function $(selector) {
  return document.querySelector(selector);
}

function $$(selector) {
  return document.querySelectorAll(selector);
}

function encodeHTML(text) {
  const div = document.createElement('div');
  div.appendChild(document.createTextNode(text));
  return div.innerHTML;
}

function addQualityIndicators() {
  $$('#output [x-bergamot-sentence-score]').forEach(el => {
    // The threshold is ln(0.5) (https://github.com/browsermt/bergamot-translator/pull/370#issuecomment-1058123399)
    el.classList.toggle('bad', parseFloat(el.getAttribute('x-bergamot-sentence-score')) < Math.log(0.5));
  });

  $$('#output [x-bergamot-word-score]').forEach(el => {
    // The threshold is ln(0.5) (https://github.com/browsermt/bergamot-translator/pull/370#issuecomment-1058123399)
    el.classList.toggle('bad', parseFloat(el.getAttribute('x-bergamot-word-score')) < Math.log(0.5));
  });

  // Add tooltips to each (sub)word with sentence and word score.
  $$('#output [x-bergamot-sentence-score] > [x-bergamot-word-score]').forEach(el => {
    const sentenceScore = parseFloat(el.parentNode.getAttribute('x-bergamot-sentence-score'));
    const wordScore = parseFloat(el.getAttribute('x-bergamot-word-score'));
    el.title = `Sentence: ${Math.exp(sentenceScore).toFixed(2)}  Word: ${Math.exp(wordScore).toFixed(2)}`;
  });
}

function highlightSentence(element) {
  const sentence = element.parentNode.hasAttribute('x-bergamot-sentence-index')
    ? element.parentNode.getAttribute('x-bergamot-sentence-index')
    : null;
  $$('#output font[x-bergamot-sentence-index]').forEach(el => {
    el.classList.toggle('highlight-sentence', el.getAttribute('x-bergamot-sentence-index') === sentence);
  })
}

/**
 * Very minimal WISYWIG editor. Just keyboard shortcuts for the IYKYK crowd.
 */
class Editor {
  constructor(root) {
    this.isApple = window.navigator.platform.startsWith('Mac');

    this.root = root;
    this.root.addEventListener('keydown', this.onkeydown.bind(this));

    this.mapping = {
      "b": "bold",
      "i": "italic",
      "u": "underline",
    };
  }

  onkeydown(event) {
    if (!(this.isApple ? event.metaKey : event.ctrlKey))
      return;

    if (!(event.key in this.mapping))
      return;

    document.execCommand(this.mapping[event.key], false, null);

    event.preventDefault();
  }
}

async function main() {
  const options = {
    cacheSize: 2^13,
    downloadTimeout: null // Disable timeout
  };
  
  const backing = new TranslatorBacking(options);

  let pending = 0; // Number of pending requests

  // Patch the fetch() function to track number of pending requests
  backing.fetch = async function(...args) {
    try {
      $('.app').classList.toggle('loading', ++pending > 0);
      return await TranslatorBacking.prototype.fetch.call(backing, ...args);
    } finally {
      $('.app').classList.toggle('loading', --pending > 0);
    }
  };

  // Wait for the language model registry to load. Once it is loaded, use
  // it to fill the "from" and "to" language selection dropdowns.
  await backing.registry.then(models => {
    const names = new Intl.DisplayNames(['en'], {type: 'language'});

    ['from', 'to'].forEach(field => {
      const languages = new Set(models.map(model => model[field]));
      const select = $(`#lang-${field}`);

      const pairs = Array.from(languages, code => ({code, name: names.of(code)}));
      
      pairs.sort(({name: a}, {name: b}) => a.localeCompare(b));

      pairs.forEach(({name, code}) => {
        select.add(new Option(name, code));
      })
    });

    $('#lang-from').value = 'en';
    $('#lang-to').value = 'es';
  });

  // Intentionally do this after querying backing.registry to make sure that
  // that request is fired off first. Now we can start thinking about loading
  // the WASM binary etc.
  const translator = new LatencyOptimisedTranslator(options, backing);

  let abortController = new AbortController();

  const translate = async () => {
    try {
      const from = $('#lang-from').value;
      const to = $('#lang-to').value;
      
      // Querying models to see whether quality estimation is supported by all
      // of them.
      const models = await backing.getModels({from, to});
      const qualityScores = models.every(model => 'qualityModel' in model.files);

      $('.app').classList.add('translating');

      const response = await translator.translate({
        from,
        to,
        text: $('#input').innerHTML,
        html: true,
        qualityScores
      }, {signal: abortController.signal});

      $('#output').innerHTML = response.target.text;
      $('#output').classList.toggle('has-quality-scores', qualityScores);

      if (qualityScores)
        addQualityIndicators();

    } catch (error) {
      // Ignore errors caused by changing the language pair (which triggers abort())
      if (error.constructor === CancelledError) {
        return;
      }
      
      // Ignore 'errors' caused by typing too fast or by changing the language
      // pair while a translation was still in progress (or being loaded)
      if (error.constructor === SupersededError || error.constructor === CancelledError)
        return;

      // Ignore errors caused by selecting a bad pair (e.g. en -> en)
      if (error.message.startsWith('No model available to translate from'))
        return;

      alert(`Error during translation: ${error}\n\n${error.stack}`);
    } finally {
      const worker = await Promise.race([translator.worker, Promise.resolve(null)]);
      $('.app').classList.toggle('translating', worker === null || !worker.idle);
    }
  }

  const reset = async () => {
    // Cancel any pending loading/translation
    abortController.abort();

    // Reset abort controller to a fresh un-aborted one
    abortController = new AbortController();

    // Clear output to make it more clear something is happening
    $('#output').innerHTML = '';

    // Immediately start loading the new selection
    translate();
  }

  $('button.swap').addEventListener('click', () => {
    const tmp = $('#lang-from').value;
    $('#lang-from').value = $('#lang-to').value;
    $('#lang-to').value = tmp;
    translate();
  })

  // Simple WYSIWYG controls
  const editor = new Editor($('#input'));

  // Translate on any change
  $('#input').addEventListener('input', translate);
  $('#lang-from').addEventListener('input', reset);
  $('#lang-to').addEventListener('input', reset);

  // Hook up sentence boundary highlighting if that information is available.
  $('#output').addEventListener('mouseover', (e) => highlightSentence(e.target))

  // Wait for bergamot-translator to load. This could throw a CompileError
  // which we want to catch so we can show "oh noes browser not supported!"
  translator.worker.catch(error => {
    // Catch CompileErrors because for those we know what to do.
    if (error.name === 'CompileError')
      $('#unsupported-browser').hidden = false;
    else
      throw error;
  });
}

main();
