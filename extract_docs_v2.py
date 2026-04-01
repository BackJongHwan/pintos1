import zipfile
import xml.etree.ElementTree as ET
import os

def get_text(docx_path):
    if not os.path.exists(docx_path):
        return f"File {docx_path} not found"
    try:
        with zipfile.ZipFile(docx_path) as z:
            xml_content = z.read('word/document.xml')
            tree = ET.fromstring(xml_content)
            content = []
            for node in tree.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}t'):
                if node.text:
                    content.append(node.text)
            return ' '.join(content)
    except Exception as e:
        return f"Error reading {docx_path}: {e}"

paths = [
    'prj1/20201587.docx',
    'prj2/20201587.docx',
    'prj3/20201587.docx',
    'prj4/20201587.docx'
]

with open('extracted_info.txt', 'w', encoding='utf-8') as f:
    for path in paths:
        f.write(f"=== {path} ===\n")
        text = get_text(path)
        f.write(text)
        f.write("\n\n")

print("Extraction complete. Results in extracted_info.txt")
