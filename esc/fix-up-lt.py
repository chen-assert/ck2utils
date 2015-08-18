#!/usr/bin/env python3

import collections
import csv
import pathlib
import re
import shutil
import tempfile
import ck2parser

rootpath = pathlib.Path('C:/Users/Nicholas/Documents/CK2')
swmhpath = rootpath / 'SWMH-BETA/SWMH'

def get_cultures():
    cultures = []
    for path in ck2parser.files('common/cultures/*.txt', swmhpath):
        tree = ck2parser.parse_file(path)
        cultures.extend(n2.val for _, v in tree for n2, v2 in v
                        if n2.val != 'graphical_cultures')
    return cultures

def process_province_history():
    def mark_barony(barony, county_set):
        try:
            if barony.val.startswith('b_'):
                county_set.add(barony.val)
        except AttributeError:
            pass

    tree = ck2parser.parse_file(swmhpath / 'map/default.map')
    defs = next(v.val for n, v in tree if n.val == 'definitions')
    id_name = {}
    with (swmhpath / 'map' / defs).open(newline='',
                                        encoding='cp1252') as csvfile:
        for row in csv.reader(csvfile, dialect='ckii'):
            try:
                id_name[int(row[0])] = row[4]
            except (IndexError, ValueError):
                continue
    province_id = {}
    used_baronies = collections.defaultdict(set)
    max_settlements = {}
    for path in ck2parser.files('history/provinces/*.txt', swmhpath):
        try:
            number, name = path.stem.split(' - ')
            id_number = int(number)
            if id_name[id_number] == name:
                tree = ck2parser.parse_file(path)
                try:
                    title = next(v.val for n, v in tree if n.val == 'title')
                except StopIteration:
                    continue
                # if id_number == 79:
                #     import pprint
                #     for n, v in tree:
                #         try:
                #             pprint.pprint(n.val)
                #         except AttributeError:
                #             pass
                #     raise SystemExit()
                for n, v in tree:
                    mark_barony(n, used_baronies[title])
                    mark_barony(v, used_baronies[title])
                    if isinstance(v, ck2parser.Obj):
                        if v.has_pairs:
                            for n2, v2 in v:
                                mark_barony(n2, used_baronies[title])
                                mark_barony(v2, used_baronies[title])
                        else:
                            for v2 in v:
                                mark_barony(v2, used_baronies[title])
                    if n.val == 'max_settlements':
                        max_settlements[title] = int(v.val)
                province_id[title] = id_number
        except:
            print(path)
            raise
    return province_id, used_baronies, max_settlements

def prepend_post_comment(item, s):
    if item.post_comment:
        s += ' ' + str(item.post_comment)
    item.post_comment = ck2parser.Comment(s)

kingdoms_for_barony_swap = [
    'k_bulgaria', 'k_serbia', 'k_bosnia', 'k_croatia', 'k_hungary',
    'k_denmark', 'k_norway', 'k_finland', 'k_pomerania', 'k_terra',
    'k_lithuania', 'k_taurica', 'k_khazaria' 'k_alania', 'k_volga_bulgaria',
    'k_bjarmia', 'k_perm']

def main():
    lt = swmhpath / 'common/landed_titles'
    province_id, used_baronies, max_settlements = process_province_history()
    localisation = ck2parser.localisation(swmhpath)
    cultures = get_cultures()
    ck2parser.fq_keys = cultures
    historical_baronies = []

    def update_tree(v):
        for n2, v2 in v:
            if isinstance(n2, ck2parser.String):
                if ck2parser.is_codename(n2.val):
                    for n3, v3 in v2:
                        if n3.val == 'capital':
                            prov_key = 'PROV{}'.format(v3.val)
                            capital_name = localisation[prov_key]
                            if not v3.post_comment:
                                v3.post_comment = ck2parser.Comment(
                                    capital_name)
                            break
                    v2.ker.post_comment = None
                    _, (nl, _) = v2.inline_str(0)
                    if nl >= 36:
                        comment = 'end ' + n2.val
                        prepend_post_comment(v2.ker, comment)
                    # if re.match(r'[ekd]_', n2.val):
                    #     try:
                    #         prepend_post_comment(v2.kel, localisation[n2.val])
                    #     except KeyError:
                    #         print('@@@ ' + n2.val)
                    baronies_to_remove = []
                    if n2.val.startswith('c_'):
                        # if v2.kel.post_comment:
                        #     print('c   ' + v2.kel.post_comment.val)
                        if (v2.kel.post_comment and
                            v2.kel.post_comment.val.isdigit()):
                            v2.kel.post_comment = None
                            try:
                                prov_id = province_id[n2.val]
                                comment = '{} ({})'.format(
                                    localisation['PROV{}'.format(prov_id)],
                                    prov_id)
                                prepend_post_comment(v2.kel, comment)
                            except KeyError:
                                print('!!! ' + n2.val)
                        num_baronies = 0
                        for child in v2.contents:
                            if child.key.val.startswith('b_'):
                                if (child.key.val in historical_baronies or
                                    child.key.val in used_baronies[n2.val]):
                                    num_baronies += 1
                                else:
                                    baronies_to_remove.append(child)
                        if (num_baronies + len(baronies_to_remove) <
                            max_settlements[n2.val]):
                            print(('{} has {} subholdings '
                                   'but {} max_settlements!').format(
                                   num_baronies + len(baronies_to_remove),
                                   max_settlements[n2.val]))
                        keep = max(0, max_settlements[n2.val] - num_baronies)
                        del baronies_to_remove[:keep]
                        v2.contents[:] = [v for v in v2.contents
                                          if v not in baronies_to_remove]
                    allow_block = None
                    for child in v2.contents:
                        if child.key.val == 'allow':
                            allow_block = child
                            break
                    if allow_block:
                        if v2.contents[-1] != allow_block:
                            v2.contents.remove(allow_block)
                            v2.contents.append(allow_block)
                        post_barony_block = allow_block
                    else:
                        post_barony_block = v2.ker
                    for barony in reversed(baronies_to_remove):
                        b_is, _ = barony.inline_str(0)
                        comments = [ck2parser.Comment(s)
                                    for s in b_is.split('\n')]
                        post_barony_block.pre_comments[0:0] = comments
                n2_lower = n2.val.lower()
                if any(n2_lower == s
                       for s in ['not', 'or', 'and', 'nand', 'nor']):
                    n2.val = n2_lower
            if isinstance(v2, ck2parser.Obj) and v2.has_pairs:
                update_tree(v2)

    for inpath in ck2parser.files('history/titles/*.txt', swmhpath):
        if inpath.stem.startswith('b_'):
            historical_baronies.append(inpath.stem)

    with tempfile.TemporaryDirectory() as td:
        lt_t = pathlib.Path(td)
        for inpath in ck2parser.files('common/landed_titles/*.txt', swmhpath):
            outpath = lt_t / inpath.name
            tree = ck2parser.parse_file(inpath)
            update_tree(tree)
            with outpath.open('w', encoding='cp1252', newline='\r\n') as f:
                f.write(tree.str())
        while lt.exists():
            print('Removing old landed_titles...')
            shutil.rmtree(str(lt), ignore_errors=True)
        shutil.copytree(str(lt_t), str(lt))

if __name__ == '__main__':
    main()
