# directory tree -> .pak (zip, deflate) with a fixed recent timestamp so its files win over the game's paks
import os, sys, zipfile
def make(src, out, stamp=(2026, 1, 1, 0, 0, 0)):
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for root, ds, fs in os.walk(src):
            for f in sorted(fs):
                p = os.path.join(root, f); a = os.path.relpath(p, src).replace(os.sep, '/')
                zi = zipfile.ZipInfo(a, date_time=stamp); zi.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(zi, open(p, 'rb').read())
    print(out, os.path.getsize(out))
if __name__ == '__main__':
    make(sys.argv[1], sys.argv[2])
