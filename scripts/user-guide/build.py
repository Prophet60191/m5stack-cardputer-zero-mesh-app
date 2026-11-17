#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the offline guides from the reviewed Markdown, without device access."""
import argparse
import hashlib
import html
import json
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

from markdown_it import MarkdownIt
from PIL import Image as PILImage
from pypdf import PdfReader, PdfWriter
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import landscape, letter
from reportlab.lib.styles import ParagraphStyle
from reportlab import rl_config
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, Frame, Image, KeepTogether, LongTable, PageBreak,
    PageTemplate, Paragraph, Spacer, TableStyle, XPreformatted,
)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = Path(__file__).resolve().parents[2]
GUIDE = ROOT / 'docs/user-guide'
SOURCES = sorted(GUIDE.glob('[0-6][0-9]-*.md'))
MD = MarkdownIt('commonmark').enable('table')
INK = colors.HexColor('#18332d')
GREEN = colors.HexColor('#087b4c')
MUTED = colors.HexColor('#53655f')
REPO = 'https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app'
# Stable timestamps/object IDs let CI reproduce the exact same downloads.
rl_config.invariant = 1


def slug(text):
    return re.sub(r'[^\w\- ]', '', text.lower()).replace(' ', '-')


def plain(tokens):
    return ''.join(t.content for t in tokens if t.type in ('text', 'code_inline'))


def headings(path):
    result = {}
    counts = {}
    tokens = MD.parse(path.read_text())
    for i, t in enumerate(tokens):
        if t.type == 'heading_open':
            label = plain(tokens[i + 1].children)
            key = slug(label)
            count = counts.get(key, 0)
            counts[key] = count + 1
            if count:
                key += '-' + str(count)
            result[i] = (label, key)
    return result


def check_sources():
    meta = json.loads((GUIDE / 'guide.json').read_text())
    version = json.loads((ROOT / 'app-builder.json').read_text())['version']
    if version != meta['app_version']:
        raise ValueError('Review the guides and update guide.json for this app version')
    if len(SOURCES) != 7:
        raise ValueError('Expected seven numbered guide chapters')
    referenced_images = set()
    for path in [GUIDE / 'README.md', *SOURCES]:
        source = path.read_text()
        if f'**{version} (release candidate)**' not in source:
            raise ValueError(f'Missing reviewed-version stamp in {path.name}')
        if meta['updated'] not in source:
            raise ValueError(f'Missing review date in {path.name}')
        if re.search(r'/Users/|file://|tailscale|ssh-rsa|BEGIN .*PRIVATE KEY', source, re.I):
            raise ValueError(f'Unexpected private/local reference in {path.name}')
        for t in MD.parse(source):
            for child in t.children or []:
                target = child.attrGet('src') if child.type == 'image' else child.attrGet('href')
                if not target:
                    continue
                url = urlsplit(target)
                if url.scheme:
                    if url.scheme != 'https':
                        raise ValueError(f'Unexpected URL scheme in {path.name}')
                    continue
                dest = (path.parent / unquote(url.path)).resolve() if url.path else path
                if not dest.is_relative_to(ROOT) or not dest.is_file():
                    raise ValueError(f'Broken local link: {path.name}: {target}')
                if url.fragment and dest.suffix == '.md':
                    anchors = {key for _, key in headings(dest).values()}
                    if unquote(url.fragment) not in anchors:
                        raise ValueError(f'Broken anchor: {path.name}: {target}')
                if child.type == 'image':
                    referenced_images.add(dest)
                    with PILImage.open(dest) as image:
                        if image.size != (320, 170) or set(image.info) - {'srgb'}:
                            raise ValueError(f'Unexpected screenshot dimensions/metadata: {dest.name}')
    expected_images = meta.get("screenshots_count", 24)
    if type(expected_images) is not int or expected_images < 1 or len(referenced_images) != expected_images:
        raise ValueError("Expected all reviewed screenshots to be referenced")
    if set((GUIDE / 'images').iterdir()) != referenced_images:
        raise ValueError('Unexpected or unreferenced files in the screenshot directory')
    print(f'Source checks: 7 chapters, {expected_images} images, local links/anchors, version {version}')
    return meta


def register_fonts(directory):
    names = {'Body': 'DejaVuSans.ttf', 'Body-Bold': 'DejaVuSans-Bold.ttf',
             'Body-Italic': 'DejaVuSans-Oblique.ttf', 'Body-BoldItalic': 'DejaVuSans-BoldOblique.ttf'}
    for name, filename in names.items():
        pdfmetrics.registerFont(TTFont(name, str(directory / filename)))
    pdfmetrics.registerFontFamily('Body', normal='Body', bold='Body-Bold',
                                  italic='Body-Italic', boldItalic='Body-BoldItalic')


class GuideDoc(BaseDocTemplate):
    def afterFlowable(self, flowable):
        if not hasattr(flowable, 'bookmark'):
            return
        key, title, level = flowable.bookmark
        self.canv.bookmarkPage(key)
        if level < 2:
            self.canv.addOutlineEntry(title, key, level, False)
            self.notify('TOCEntry', (level, html.escape(title), self.page, key))


class Renderer:
    def __init__(self, meta, compact=False):
        self.meta = meta
        self.compact = compact
        self.width = 349 if compact else 504
        size = 8.5 if compact else 10
        leading = 11.1 if compact else 14.2
        self.body = ParagraphStyle('Body', fontName='Body', fontSize=size, leading=leading,
                                   textColor=INK, spaceAfter=4 if compact else 7)
        self.cell = ParagraphStyle('Cell', parent=self.body, fontSize=8 if compact else 9,
                                   leading=10.4 if compact else 12.5, spaceAfter=0)
        self.small = ParagraphStyle('Small', parent=self.body, fontSize=8.3, leading=11,
                                    textColor=MUTED, spaceAfter=8)
        self.h = {
            1: ParagraphStyle('H1', parent=self.body, fontName='Body-Bold', fontSize=23,
                              leading=28, textColor=GREEN, spaceAfter=16, keepWithNext=True),
            2: ParagraphStyle('H2', parent=self.body, fontName='Body-Bold', fontSize=10 if compact else 14,
                              leading=13 if compact else 18, spaceBefore=7 if compact else 14,
                              spaceAfter=5 if compact else 8, keepWithNext=True),
            3: ParagraphStyle('H3', parent=self.body, fontName='Body-Bold', fontSize=8.8 if compact else 11,
                              leading=11.3 if compact else 15, spaceBefore=5 if compact else 10,
                              spaceAfter=3 if compact else 5, keepWithNext=True),
        }

    def link(self, target, path):
        url = urlsplit(target)
        if url.scheme:
            return target
        dest = (path.parent / url.path).resolve() if url.path else path
        if self.compact:
            return REPO + '/blob/main/' + dest.relative_to(ROOT).as_posix() + ('#' + url.fragment if url.fragment else '')
        if dest in SOURCES:
            return '#' + dest.stem + ('--' + url.fragment if url.fragment else '')
        if dest == GUIDE / 'README.md':
            return '#contents'
        return REPO + '/blob/main/' + dest.relative_to(ROOT).as_posix() + ('#' + url.fragment if url.fragment else '')

    def inline(self, tokens, path):
        result = []
        for t in tokens:
            if t.type in ('text', 'code_inline'):
                # Use the embedded Unicode font for key labels as well as prose.
                text = t.content.replace('\u2011', '-').replace('\u2013', '-').replace('\u2014', ' - ')
                # DejaVu has no color emoji; spell out the position symbol.
                text = text.replace('📍', 'GPS:')
                missing = set(text) - set(map(chr, pdfmetrics.getFont('Body').face.charWidths)) - {'\n', '\r'}
                if missing:
                    raise ValueError(f'Font does not cover characters: {sorted(missing)}')
                result.append(html.escape(text))
            elif t.type in ('strong_open', 'strong_close', 'em_open', 'em_close'):
                tag = 'b' if t.type.startswith('strong') else 'i'
                result.append('<' + ('/' if t.nesting == -1 else '') + tag + '>')
            elif t.type == 'link_open':
                result.append(f'<a href="{html.escape(self.link(t.attrGet("href"), path), quote=True)}" color="#087b4c">')
            elif t.type == 'link_close':
                result.append('</a>')
            elif t.type == 'softbreak':
                result.append(' ')
            elif t.type == 'hardbreak' or (t.type == 'html_inline' and t.content in ('<br>', '<br/>', '<br />')):
                result.append('<br/>')
            elif t.type == 'image':
                raise ValueError('Inline images must be standalone screenshot paragraphs')
            else:
                raise ValueError(f'Unsupported inline Markdown: {t.type}')
        return ''.join(result)

    def table(self, tokens, start, path):
        rows, cells, row = [], [], []
        i = start + 1
        while tokens[i].type != 'table_close':
            t = tokens[i]
            if t.type == 'tr_open':
                row = []
            elif t.type in ('th_open', 'td_open'):
                cells = []
            elif t.type == 'inline':
                cells.append(self.inline(t.children, path))
            elif t.type in ('th_close', 'td_close'):
                row.append(Paragraph(''.join(cells) or ' ', self.cell))
            elif t.type == 'tr_close':
                rows.append(row)
            i += 1
        n = len(rows[0])
        fractions = [0.32, 0.68] if n == 2 else [0.25, 0.16, 0.59]
        table = LongTable(rows, colWidths=[self.width * x for x in fractions], repeatRows=1,
                          hAlign='LEFT', spaceAfter=5 if self.compact else 10)
        table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e4efe9')),
            ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, colors.HexColor('#f5f8f6')]),
            ('VALIGN', (0, 0), (-1, -1), 'TOP'),
            ('LEFTPADDING', (0, 0), (-1, -1), 5), ('RIGHTPADDING', (0, 0), (-1, -1), 5),
            ('TOPPADDING', (0, 0), (-1, -1), 3 if self.compact else 6),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 3 if self.compact else 6),
            ('LINEBELOW', (0, 0), (-1, 0), 0.5, GREEN),
        ]))
        return table, i

    def chapter(self, path):
        tokens = MD.parse(path.read_text())
        anchors = headings(path)
        output, lists = [], []
        i = 0
        while i < len(tokens):
            t = tokens[i]
            if t.type == 'blockquote_open':  # The source review stamp is in the PDF header.
                while tokens[i].type != 'blockquote_close':
                    i += 1
            elif t.type == 'heading_open':
                level = int(t.tag[1])
                label, anchor = anchors[i]
                if not (self.compact and level == 1):
                    p = Paragraph(self.inline(tokens[i + 1].children, path), self.h[min(level, 3)])
                    if not self.compact:
                        p.bookmark = (path.stem if level == 1 else path.stem + '--' + anchor, label, level - 1)
                    output.append(p)
                i += 2
            elif t.type in ('bullet_list_open', 'ordered_list_open'):
                lists.append(None if t.type == 'bullet_list_open' else int(t.attrGet('start') or 1))
            elif t.type in ('bullet_list_close', 'ordered_list_close'):
                lists.pop()
            elif t.type == 'list_item_open':
                marker = '•' if lists[-1] is None else str(lists[-1]) + '.'
                if lists[-1] is not None:
                    lists[-1] += 1
                # Only the first paragraph of each list item carries its marker.
                t.meta['marker'] = marker
            elif t.type == 'paragraph_open':
                inline = tokens[i + 1]
                content = inline.content
                if content.startswith(('For Mesh ', '**Guide Index:**')) or '←' in content or content.startswith('[Quick Reference]'):
                    i += 2
                    i += 1
                    continue
                images = [x for x in inline.children if x.type == 'image']
                if images:
                    if len(inline.children) != 1:
                        raise ValueError('Expected a standalone screenshot')
                    im = images[0]
                    figure = Image(str(path.parent / im.attrGet('src')), width=352, height=187)
                    caption = Paragraph(html.escape(im.content), ParagraphStyle('Caption', parent=self.small, alignment=TA_CENTER))
                    group = [Spacer(1, 4), figure, Spacer(1, 5), caption]
                    if output and isinstance(output[-1], Paragraph) and getattr(output[-1].style, 'keepWithNext', False):
                        group.insert(0, output.pop())
                    output.append(KeepTogether(group))
                else:
                    style = self.body
                    prefix = ''
                    if lists:
                        style = ParagraphStyle('List', parent=self.body, leftIndent=12 * len(lists), firstLineIndent=0)
                        # Look backward only through the current item prefix.
                        if i > 0 and tokens[i - 1].type == 'list_item_open':
                            prefix = html.escape(tokens[i - 1].meta['marker']) + ' '
                    output.append(Paragraph(prefix + self.inline(inline.children, path), style))
                i += 2
            elif t.type == 'table_open':
                table, i = self.table(tokens, i, path)
                output.append(table)
            elif t.type in ('fence', 'code_block'):
                output.append(XPreformatted(html.escape(t.content), self.body))
            elif t.type not in ('hr', 'list_item_close'):
                raise ValueError(f'Unsupported block Markdown: {t.type}')
            i += 1
        return output


def page_style(meta, compact):
    def draw(canvas, doc):
        w, h = doc.pagesize
        canvas.setStrokeColor(GREEN)
        canvas.setLineWidth(1.2)
        canvas.line(36 if compact else 54, h - 33, w - (36 if compact else 54), h - 33)
        canvas.setFillColor(MUTED)
        canvas.setFont('Body', 8)
        canvas.drawString(36 if compact else 54, h - 25, 'CARDPUTER ZERO  /  MESH')
        canvas.drawRightString(w - (36 if compact else 54), h - 25,
                              f'{meta["app_version"]} CANDIDATE  |  {meta["updated"]}')
        canvas.drawString(36 if compact else 54, 23, 'Synthetic examples. Embedded font license notices are attached.')
        canvas.drawRightString(w - (36 if compact else 54), 23, str(doc.page))
    return draw


def finish_pdf(raw, output, title):
    writer = PdfWriter(clone_from=raw)
    writer.add_attachment('DejaVu-LICENSE.txt', (ROOT / 'scripts/user-guide/DejaVu-LICENSE.txt').read_bytes())
    writer.add_metadata({'/Title': title, '/Author': 'Cardputer Zero Mesh contributors',
                         '/Subject': 'Mesh user documentation, synthetic examples', '/Creator': 'Mesh Markdown guide builder'})
    with output.open('wb') as stream:
        writer.write(stream)
    raw.unlink()


def build(meta, output):
    output.mkdir(parents=True, exist_ok=True)
    quick = Renderer(meta, compact=True)
    raw = output / '.quick-reference.pdf'
    doc = BaseDocTemplate(str(raw), pagesize=landscape(letter), pageCompression=1,
                          title='Quick Reference Card', author='Cardputer Zero Mesh contributors')
    frames = [Frame(x, 37, 349, 526, leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0) for x in (36, 407)]
    doc.addPageTemplates(PageTemplate('Quick', frames, onPage=page_style(meta, True)))
    story = [Paragraph('Quick Reference Card', quick.h[1]), *quick.chapter(SOURCES[0])]
    doc.build(story)
    if len(PdfReader(raw).pages) != 1:
        raise ValueError('Quick reference overflow: edit the source/layout; do not shrink it automatically')
    finish_pdf(raw, output / 'Quick-Reference-Card.pdf', 'Cardputer Zero Mesh - Quick Reference Card')

    render = Renderer(meta)
    raw = output / '.user-guide.pdf'
    doc = GuideDoc(str(raw), pagesize=letter, pageCompression=1,
                   title='Cardputer Zero Mesh User Guide', author='Cardputer Zero Mesh contributors')
    frame = Frame(54, 43, 504, 706, leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    doc.addPageTemplates(PageTemplate('Manual', [frame], onPage=page_style(meta, False)))
    story = [Spacer(1, 65), Paragraph('Cardputer Zero<br/>Mesh User Guide', ParagraphStyle('Cover', parent=render.h[1], fontSize=34, leading=42)),
             Paragraph('A practical guide to your handheld mesh radio', render.h[2]),
             Spacer(1, 18), Image(str(GUIDE / 'images/home-carousel.png'), width=384, height=204),
             Spacer(1, 22), Paragraph(f'For Mesh {meta["app_version"]} (release candidate)<br/>Reviewed {meta["updated"]}', render.body),
             Paragraph('Keyboard controls, messages, nodes, map, settings and troubleshooting.', render.body),
             Paragraph('All examples are synthetic. Screenshots were rendered with ' + meta['screenshots_version'] + '; controls were reviewed against ' + meta['app_version'] + '.', render.small),
             Paragraph('Meshtastic is a registered trademark of Meshtastic LLC. This independent community project is not endorsed by Meshtastic LLC or M5Stack.', render.small), PageBreak()]
    contents = Paragraph('Contents', render.h[1])
    contents.bookmark = ('contents', 'Contents', 0)
    story.append(contents)
    toc = TableOfContents()
    toc.levelStyles = [ParagraphStyle('TOC0', parent=render.body, fontName='Body-Bold', fontSize=11, leading=17, spaceBefore=9),
                       ParagraphStyle('TOC1', parent=render.small, leftIndent=12, leading=11.5, spaceAfter=1)]
    story.append(toc)
    for path in [*SOURCES[1:], SOURCES[0]]:
        story.extend([PageBreak(), *render.chapter(path)])
    doc.multiBuild(story)
    finish_pdf(raw, output / 'Cardputer-Zero-Mesh-User-Guide.pdf', 'Cardputer Zero Mesh - User Guide')

    summaries = []
    for path in sorted(output.glob('*.pdf')):
        reader = PdfReader(path)
        text = '\n'.join(page.extract_text() for page in reader.pages)
        if meta['app_version'] not in text or len(text) < 2000:
            raise ValueError(f'Incomplete PDF: {path.name}')
        if '/Users/' in text or '\ufffd' in text:
            raise ValueError(f'Unexpected private path or replacement glyph in {path.name}')
        if 'DejaVu-LICENSE.txt' not in reader.attachments:
            raise ValueError('Missing embedded-font notices')
        summaries.append({'file': path.name, 'pages': len(reader.pages), 'bytes': path.stat().st_size,
                          'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    manifest = {'app_version': meta['app_version'], 'reviewed': meta['updated'], 'files': summaries}
    (output / 'guide-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (output / 'SHA256SUMS').write_text(''.join(f'{item["sha256"]}  {item["file"]}\n' for item in summaries))
    print(json.dumps(manifest, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/user-guide')
    parser.add_argument('--font-dir', type=Path, default=Path('/usr/share/fonts/truetype/dejavu'))
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    meta = check_sources()
    if not args.check_only:
        register_fonts(args.font_dir)
        build(meta, args.output.resolve())


if __name__ == '__main__':
    main()
