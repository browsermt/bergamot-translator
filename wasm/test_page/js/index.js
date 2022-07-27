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

async function main() {
  try {
    const translator = new BergamotLatencyOptimisedTranslator({
      cacheSize: 2^13,
      workerUrl: 'js/translator-worker.js'
    });

    // Not strictly necessary, but useful for early error detection
    await translator.initialize();

    const translate = async () => {
      try {
        const from = $('#lang-from').value;
        const to = $('#lang-to').value;
        
        // Querying models to see whether quality estimation is supported by all
        // of them.
        const models = await translator.getModels({from, to});
        const qualityScores = models.every(model => 'qualityModel' in model.files);

        $('.app').classList.add('loading');

        const response = await translator.translate({
          from,
          to,
          text: $('#input').innerHTML,
          html: true,
          qualityScores
        });

        $('#output').innerHTML = response.target.text;
        $('#output').classList.toggle('has-quality-scores', qualityScores);

        if (qualityScores)
          addQualityIndicators();

      } catch (error) {
        if (error.constructor === SupersededError)
          return;
        
        alert(`Error during translation: ${error}\n${error.stack}`);
      } finally {
        $('.app').classList.toggle('loading', translator.worker && !translator.worker.idle);
      }
    }

    translator.registry.then(models => {
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

    $('button.swap').addEventListener('click', () => {
      const tmp = $('#lang-from').value;
      $('#lang-from').value = $('#lang-to').value;
      $('#lang-to').value = tmp;
      translate();
    })

    $('#input').addEventListener('input', translate);
    $('#lang-from').addEventListener('input', translate);
    $('#lang-to').addEventListener('input', translate);

    $('#output').addEventListener('mouseover', (e) => highlightSentence(e.target))
  } catch (error) {
    if (error.name === 'CompileError')
      $('#unsupported-browser').hidden = false;
    else
      throw error;
  }
}

main();
