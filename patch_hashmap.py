import re
text = open('std/collections.alu').read()
text = re.sub(r'm.buckets = alu_alloc\(m.capacity \* 24\);', 'm.buckets = alu_alloc(m.capacity * 8);', text)
text = re.sub(r'HashEntry<K, V> e = b\[i\];', 'HashEntry<K, V> e = new HashEntry<K, V>;', text)
text = re.sub(r'HashEntry<K, V> e = new_b\[i\];', 'HashEntry<K, V> e = new HashEntry<K, V>;', text)
open('std/collections.alu', 'w').write(text)
