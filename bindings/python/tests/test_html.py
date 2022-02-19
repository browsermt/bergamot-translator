# type: ignore
from bergamot import REPOSITORY, ResponseOptions, Service, ServiceConfig, VectorString
import pytest
from collections import Counter
from string import whitespace

try:
    from lxml import etree, html
except:
    raise ImportError("Please install lxml, the html tests require these")


def test_html():
    MODEL = 'en-de-tiny'
    config = ServiceConfig(numWorkers=4, logLevel='warn')
    service = Service(config)
    config_path = REPOSITORY.modelConfigPath('browsermt', MODEL)
    model = service.modelFromConfigPath(config_path)

    example ="""
<div class="wrap">
    <div class="image-wrap"><img src="/images/about.jpg" alt=""></div>
    <h2 id="the-bergamot-project">Das Projekt Bergamot</h2>
    <p>Das Bergamot-Projekt wird die Übersetzung der Client-Seite der Maschine in einem Webbrowser ergänzen und verbessern.</p>
    <p>Im Gegensatz zu aktuellen <b>Cloud-basierten Optionen</b>, die di<s>rekt</s> auf den Rechnern der Nutzer laufen, die Bürgerinnen und Bürger, ihre Privatsphäre zu bewahren und erhöht die Verbreitung von Sprachtechnologien in Europa in verschiedenen Sektoren, die Vertraulichkeit erfordern. Freie Software, die mit einem Open-Source-Webbrowser wie Mozilla Firefox integriert ist, wird die Akzeptanz von unten nach oben durch Nicht-Experten ermöglichen, was zu Kosteneinsparungen für private und öffentliche Nutzer führt, die andernfalls Übersetzungen beschaffen oder einsprachig arbeiten würden.</p>
    <p>Bergamot ist ein Konsortium, das von der Universität Edinburgh mit den Partnern Charles University in Prag, der University of Sheffield, der University of Tartu und Mozilla koordiniert wird.</p>
</div>
"""

    def translate(src, HTML=True):
        options = ResponseOptions(HTML=HTML)
        responses = service.translate(model, VectorString([src]), options)
        return responses[0].target.text

    def get_surrounding_text(el):
        """
        Places for spaces: 0 <b> 1 … 2 </b> 3
        0 before_open: prev.tail[-1] if prev else parent.text[-1]
        1 after_open: elem.text[0]
        2 before_close: last_child.tail[-1] if last_child else elem.text[-1]
        3 after_close: elem.tail[0]
        """
        before_open = el.getprevious().tail if el.getprevious() is not None else el.getparent().text
        after_open = el.text
        last_child = next(el.iterchildren(reversed=True), None)
        before_close = last_child.tail if last_child is not None else el.text
        after_close = el.tail
        return [before_open, after_open, before_close, after_close]

    def has_surrounding_text(el):
        return [
            text is not None and text.strip() != ""
            for text in get_surrounding_text(el)
        ]

    def has_surrounding_spaces(el):
        return [
            isinstance(text, str) and len(text) > 0 and text[index] in whitespace
            for text, index in zip(get_surrounding_text(el), [-1, 0, -1, 0])
        ]


    def format_whitespace(slots):
        return '{}<t>{}…{}</t>{}'.format(*[
            '␣' if slot else '⊘'
            for slot in slots
        ])

    def format_element(el):
        return '<{}{}>'.format(el.tag, ''.join(
            f' {key}="{val}"'
            for key, val in el.items()
            if key != 'x-test-id'
        ))

    def clean_html(src):
        tree = html.fromstring(src)
        return html.tostring(tree, encoding='utf-8').decode()

    def compare_html(src, translate):
        """Marks tags, then translates and compares translated HTML"""
        src_tree = html.fromstring(src)
        src_elements = {
            str(n): element for n, element in enumerate(src_tree.iter(), 1)
        }
        # Assign each element a unique id to help it correlate after translation
        for n, element in src_elements.items():
            element.set('x-test-id', n)
        src = html.tostring(src_tree, encoding='utf-8').decode()

        tgt = translate(src)
        tgt_tree = html.fromstring(tgt)

        # Test if all elements are referenced once
        print("Elements referenced:")
        tgt_element_count = Counter(element.get('x-test-id') for element in tgt_tree.iter())
        for n, element in src_elements.items():
            count = tgt_element_count[n]
            if count != 1:
                print(f"{count}: {element!r}")

        # Test whether all elements have text around them (i.e. no empty elements
        # that should not be empty)
        print("Elements with missing text:")
        for tgt_element in tgt_tree.iter():
            n = tgt_element.get('x-test-id')
            src_element = src_elements[n]
            tgt_text = has_surrounding_text(tgt_element)
            src_text = has_surrounding_text(src_element)
            if tgt_text != src_text:
                print(f"{element!r}: {tgt_text!r} (input: {src_text!r})")

        # Test whether the spaces around the elements are present. All spaces are
        # treated as a single space (unless <pre></pre>) thus it doesn't need to
        # be exactly the same. But space vs no space does affect the flow of the
        # document.
        print("Elements with differences in whitespace around tags")
        for tgt_element in tgt_tree.iter():
            n = tgt_element.get('x-test-id')
            src_element = src_elements[n]
            tgt_spaces = has_surrounding_spaces(tgt_element)
            src_spaces = has_surrounding_spaces(src_element)
            if tgt_spaces != src_spaces:
                print(f"{format_whitespace(tgt_spaces)} (input: {format_whitespace(src_spaces)}) for {format_element(tgt_element)}")


    compare_html(example, translate)
