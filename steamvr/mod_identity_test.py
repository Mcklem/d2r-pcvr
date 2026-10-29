#!/usr/bin/env python3
"""Whose files are in a game folder, and the plugin's own identity; temporary fixtures only."""
import json
from pathlib import Path
import re
import tempfile
import unittest

import mod_identity as ids

ROOT = Path(__file__).resolve().parent.parent
# Byte patterns as the real plugins carry them (checked against both mods' release DLLs).
D2R_VR_PLUGIN = b'MZ fixture ... BodyWalk Game Link ... ' + 'd2r_vr_skill_icons'.encode('utf-16-le')
OLD_PCVR_PLUGIN = b'MZ fixture d2r-vr-vrcam ' + 'd2r_vr_icons'.encode('utf-16-le')
NEW_PCVR_PLUGIN = b'MZ fixture d2r-pcvr-vrcam ' + 'd2r_vr_icons'.encode('utf-16-le')


class SurveyTests(unittest.TestCase):
    def put(self, path, data=b'fixture'):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def snapshot(self, folder, game, installed):
        self.put(folder / 'manifest.json', json.dumps({'game': str(game), 'installed': [str(p) for p in installed]}).encode())
        return folder

    def old_install(self, root, record=True):
        """D2R PCVR 0.3.15 or older: D2R VR's file names, recorded by its own installer."""
        game = root / 'game'
        self.put(game / ids.LEGACY_PLUGIN, OLD_PCVR_PLUGIN)
        self.put(game / ids.LEGACY_SETTINGS, b'[body_calibration]\nheight=1.83\n')
        snapshot = self.snapshot(root / 'backups/20261001-120000-000000', game,
                                 [ids.RECORD, ids.LEGACY_PLUGIN, ids.LEGACY_SETTINGS])
        if record:
            self.put(game / ids.RECORD, json.dumps({'game': str(game), 'snapshots': [str(snapshot)]}).encode())
        return game, snapshot

    def test_plugin_owner_reads_marks_not_names(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for data, owner in ((D2R_VR_PLUGIN, ids.OTHER_MOD), (OLD_PCVR_PLUGIN, ids.MOD_ID),
                                (NEW_PCVR_PLUGIN, ids.MOD_ID), (b'some other plugin', 'unknown')):
                with self.subTest(owner=owner, data=data[:30]):
                    self.assertEqual(ids.plugin_owner(self.put(root / 'd2rl-vrcam.dll', data)), owner)
            self.assertIsNone(ids.plugin_owner(root / 'missing.dll'))

    def test_clean_game_has_nothing(self):
        with tempfile.TemporaryDirectory() as directory:
            game = Path(directory)
            self.assertEqual(ids.survey(game), ids.Survey(ours=False, legacy=False, other=None, message=''))

    def test_old_release_under_old_names_is_ours_by_record(self):
        with tempfile.TemporaryDirectory() as directory:
            game, _ = self.old_install(Path(directory))
            found = ids.survey(game)
            self.assertEqual((found.ours, found.legacy, found.other), (True, True, None))

    def test_old_release_without_record_is_ours_only_with_its_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game, snapshot = self.old_install(root, record=False)
            found = ids.survey(game)
            self.assertEqual((found.ours, found.legacy, found.other), (False, False, 'unknown'))
            self.assertIn('did not install', found.message)
            for folder in (snapshot, snapshot.parent):
                with self.subTest(folder=folder):
                    found = ids.survey(game, [folder])
                    self.assertEqual((found.ours, found.legacy, found.other), (True, True, None))

    def test_record_without_readable_snapshots_still_means_an_old_release(self):
        with tempfile.TemporaryDirectory() as directory:
            game, snapshot = self.old_install(Path(directory))
            (snapshot / 'manifest.json').unlink()
            self.assertTrue(ids.survey(game).legacy)

    def test_d2r_vr_is_never_ours_with_or_without_our_record(self):
        for record in (False, True):
            with self.subTest(record=record), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                game, snapshot = self.old_install(root, record)
                # D2R VR installed over the old D2R PCVR, or alone.
                self.put(game / ids.LEGACY_PLUGIN, D2R_VR_PLUGIN)
                self.put(game / ids.LEGACY_SETTINGS, b'[mode]\nplatform=1\n')
                found = ids.survey(game, [snapshot.parent])
                self.assertEqual((found.ours, found.legacy, found.other), (False, False, ids.OTHER_MOD))
                self.assertIn('D2R VR', found.message)
                self.assertIn('uninstaller', found.message)

    def test_after_the_rename_the_old_names_are_never_ours(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game, snapshot = self.old_install(root)
            self.put(game / ids.PLUGIN, NEW_PCVR_PLUGIN)
            found = ids.survey(game)
            # Even a plugin with this mod's marks: it was copied there by hand.
            self.assertEqual((found.ours, found.legacy, found.other), (True, False, 'unknown'))
            (game / ids.PLUGIN).unlink()
            # The newest snapshot installed the new name, then the new plugin went missing.
            self.snapshot(root / 'backups/20261015-120000-000000', game, [ids.PLUGIN, ids.SETTINGS])
            found = ids.survey(game, [root / 'backups'])
            self.assertEqual((found.legacy, found.other), (False, 'unknown'))

    def test_snapshots_of_another_game_folder_do_not_count(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game, snapshot = self.old_install(root, record=False)
            self.snapshot(root / 'other/20261001', root / 'other game', [ids.LEGACY_PLUGIN])
            self.assertEqual(ids.survey(game, [root / 'other']).other, 'unknown')

    def test_old_settings_alone_are_carried_when_the_record_says_so(self):
        with tempfile.TemporaryDirectory() as directory:
            game, _ = self.old_install(Path(directory))
            (game / ids.LEGACY_PLUGIN).unlink()
            found = ids.survey(game)
            self.assertEqual((found.ours, found.legacy, found.other), (True, True, None))


class IdentityTests(unittest.TestCase):
    """The plugin says it is D2R PCVR everywhere, with one version."""

    def header(self):
        text = (ROOT / 'vr/pcvr_identity.h').read_text(encoding='utf-8')
        return dict(re.findall(r'#define (D2RPCVR_\w+) (.+)', text))

    def test_version_agrees_across_plugin_installer_and_build_script(self):
        values = self.header()
        version = values['D2RPCVR_VERSION'].strip('"')
        self.assertRegex(version, r'^\d+\.\d+\.\d+$')
        self.assertEqual(values['D2RPCVR_VERSION_RC'], ', '.join(version.split('.') + ['0']))
        setup = (ROOT / 'installer/D2R_PCVR_Setup.iss').read_text(encoding='utf-8')
        self.assertIn(f'#define ModVersion "{version}"', setup)
        build = (ROOT / 'installer/build_native_installer.ps1').read_text(encoding='utf-8')
        self.assertIn(f"[string]$Version = '{version}'", build)

    def test_launcher_manifest_agrees_with_the_mod_and_ships(self):
        # vrmod-manifest.json is what the D2R VR Launcher reads from mods\D2R-PCVR; it must not drift from the release.
        manifest = json.loads((ROOT / 'vrmod-manifest.json').read_text(encoding='utf-8'))
        self.assertEqual(manifest['version'], self.header()['D2RPCVR_VERSION'].strip('"'))
        self.assertEqual((manifest['id'], manifest['d2rloader_mod']['name'], manifest['name']),
                         (ids.MOD_ID, ids.MOD_NAME, ids.DISPLAY_NAME))
        for name in ('steamvr/package.py', 'installer/stage_native_installer.py'):
            self.assertTrue("'vrmod-manifest.json'" in (ROOT / name).read_text(encoding='utf-8'), name)

    def test_identity_is_not_d2r_vr_and_keeps_the_credit(self):
        values = self.header()
        self.assertEqual(values['D2RPCVR_MOD_ID'], '"d2r-pcvr"')
        self.assertEqual(values['D2RPCVR_PLUGIN_ID'], '"d2r-pcvr-vrcam"')
        self.assertEqual(values['D2RPCVR_NAME'], '"D2R PCVR"')
        self.assertEqual(values['D2RPCVR_PLUGIN_FILE'], '"d2rl-pcvr.dll"')
        self.assertEqual(values['D2RPCVR_SETTINGS_FILE_W'], 'L"d2r_pcvr.ini"')
        self.assertIn('BodyWalkVR', values['D2RPCVR_CREDITS'])
        source = (ROOT / 'vr/vrcam.cpp').read_text(encoding='utf-8')
        # assertTrue: a failure must not print the whole plugin source.
        self.assertTrue('D2RL_PLUGIN_ABI_VERSION, D2RPCVR_PLUGIN_ID, D2RPCVR_NAME, g_info_version, D2RPCVR_AUTHOR' in source)
        self.assertTrue('wcscat_s(g_iniPath, D2RPCVR_SETTINGS_FILE_W);' in source)
        self.assertFalse('L"d2r_vr.ini"' in source)
        rc = (ROOT / 'vr/vrcam.rc').read_text(encoding='utf-8')
        for value in ('D2RPCVR_VERSION_RC', 'D2RPCVR_NAME', 'D2RPCVR_PLUGIN_FILE', 'BodyWalkVR'):
            self.assertIn(value, rc)
        self.assertTrue((ROOT / 'LICENSE').read_text(encoding='utf-8').startswith('Mozilla Public License Version 2.0'))
        self.assertIn('BodyWalkVR', (ROOT / 'NOTICE').read_text(encoding='utf-8'))
        self.assertIn(ids.PLUGIN.name, values['D2RPCVR_PLUGIN_FILE'])

    def test_release_lists_ship_the_new_names_only(self):
        for name in ('steamvr/package.py', 'installer/stage_native_installer.py', 'steamvr/setup_windows.py'):
            with self.subTest(name=name):
                text = (ROOT / name).read_text(encoding='utf-8')
                self.assertIn('bin/d2rl-pcvr.dll', text)
                self.assertIn('mod_identity.py', text)
                self.assertNotIn('d2rl-vrcam', text)
                self.assertNotIn("'d2r_vr.ini'", text)
        self.assertTrue((ROOT / 'd2r_pcvr.ini').is_file())
        self.assertFalse((ROOT / 'd2r_vr.ini').exists())
        self.assertIn('add_library(d2rl-pcvr SHARED', (ROOT / 'steamvr/CMakeLists.txt').read_text(encoding='utf-8'))

    def test_release_lists_ship_both_licence_files(self):
        # LICENSE is the MPL; NOTICE holds the MIT notice of the code released before it.
        for name in ('steamvr/package.py', 'installer/stage_native_installer.py'):
            with self.subTest(name=name):
                text = (ROOT / name).read_text(encoding='utf-8')
                self.assertIn("'LICENSE', 'NOTICE'", text)

    def test_d2r_vr_installer_leftovers_are_gone(self):
        # D2R VR's own Inno script (its AppId) and build script are not this mod's.
        self.assertFalse((ROOT / 'installer/D2R_VR_Setup.iss').exists())
        self.assertFalse((ROOT / 'installer/build_installer.ps1').exists())



class ModFolderTests(unittest.TestCase):
    def test_everything_of_the_mod_is_in_its_folder(self):
        for path in (ids.PLUGIN, ids.SETTINGS, ids.RESHADE, ids.OPENXR_LOADER, ids.MODINFO):
            self.assertEqual(path.parts[:2], ('mods', 'D2R-PCVR'), path)
        info = json.loads(ids.modinfo_bytes())
        self.assertEqual((info['name'], info['savepath']), ('D2R-PCVR', '../'))
        self.assertIn('D2R VR Unleashed', info['description'])

    def test_loader_files_are_told_from_the_mod_files(self):
        for name in ('D2RLoader.exe', 'D2RCore.dll', 'd2rloader.mpq', 'd2rloader/config/d2rloader.toml', 'mods/Other/x.txt'):
            self.assertTrue(ids.loader_file(Path(name)), name)
        for name in (ids.PLUGIN, ids.SETTINGS, ids.RESHADE, ids.RECORD, ids.LEGACY_PLUGIN, ids.GLOBAL_PLUGIN,
                     Path('d2rloader/runtime/d3dcompiler_47.dll'), Path('ReShade64.dll'), Path('openxr_loader.dll')):
            self.assertFalse(ids.loader_file(name), name)

    def test_other_loader_users(self):
        with tempfile.TemporaryDirectory() as temporary:
            game = Path(temporary)
            (game / 'mods/D2R-PCVR/d2rloader/plugins').mkdir(parents=True)
            (game / 'mods/Empty').mkdir()
            (game / 'd2rloader/plugins').mkdir(parents=True)
            (game / ids.GLOBAL_PLUGIN).write_bytes(NEW_PCVR_PLUGIN)
            self.assertEqual(ids.other_loader_users(game), [])
            (game / 'mods/D2R-VR/d2rloader/plugins').mkdir(parents=True)
            (game / 'd2rloader/plugins/another.dll').write_bytes(b'MZ another')
            self.assertEqual(sorted(ids.other_loader_users(game)), ['D2R-VR', 'another.dll'])

if __name__ == '__main__':
    unittest.main()
