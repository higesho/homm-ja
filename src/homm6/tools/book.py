# Write a Heroes VI localization workbook (Excel 2003 XML, same layout as the game's Data17 files)
from xml.sax.saxutils import escape

HEAD = '''﻿<?xml version="1.0" encoding="utf-8"?>
<!--Might & Magic - Heroes VI-->
<Workbook xmlns="urn:schemas-microsoft-com:office:spreadsheet" xmlns:o="urn:schemas-microsoft-com:office:office" xmlns:x="urn:schemas-microsoft-com:office:excel" xmlns:ss="urn:schemas-microsoft-com:office:spreadsheet" xmlns:html="http://www.w3.org/TR/REC-html40">
  <Styles>
    <Style ss:ID="sheader">
      <Alignment ss:Horizontal="Justify" ss:Vertical="Top" ss:WrapText="1" />
      <Font ss:Bold="1" />
    </Style>
    <Style ss:ID="s1">
      <Alignment ss:Horizontal="Justify" ss:Vertical="Top" ss:WrapText="1" />
    </Style>
  </Styles>
  <Worksheet ss:Name="Sheet1">
    <Table>
'''
TAIL = '''    </Table>
  </Worksheet>
</Workbook>
'''
COLS = ['Text to translate', 'Translated text (Edit only this column)', 'Text identifier', 'Category', 'Description']

def esc(s):
    return escape(s, {'"': '&quot;', '\r': '&#13;', '\n': '&#10;'})

def row(cells, style):
    out = ['      <Row ss:StyleID="%s">\n' % style]
    for c in cells:
        out.append('        <Cell>\n          <Data ss:Type="String">%s</Data>\n        </Cell>\n' % esc(c))
    out.append('      </Row>\n')
    return ''.join(out)

def make(rows):
    """rows: list of (en, tr, id, cat, desc)"""
    parts = [HEAD, '      <Column ss:Width="350" />\n' * 5, row(COLS, 'sheader')]
    for r in rows: parts.append(row(r, 's1'))
    parts.append(TAIL)
    return ''.join(parts).encode('utf-8')
