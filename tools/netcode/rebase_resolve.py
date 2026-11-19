# Resolves one step of the piccolo rebase (netcode onto main). Run from the worktree root
# while "git rebase origin/main" is stopped on a conflict; then "git rebase --continue" and repeat.
# Needs the libraries rebased first (frontends-common netcode on its origin/main).
# Branch-only helper: delete with NETCODE-PENDING.md before merging to main.
import subprocess, re, sys
def git(*a, cwd='.'):
    return subprocess.run(['git', *a], cwd=cwd, capture_output=True, text=True).stdout.strip()
FC='deps/common/frontends-common'
# old piccolo commit being applied -> its subject -> the rebased library commit with the same subject
step_subject = {
 'Network listener': 'frontend_net: one UDP port with ENet and LAN discovery',
 'Network: encrypted connections': 'frontend_net: encrypted links (Noise XX on Monocypher)',
 'NETWORK window': 'frontend_net: pairing with a 6-digit code (CPace), paired devices',
 'Remote input': "Remote pads: net_input packets and input_net, the router's network backend",
 'Remote pads only play': 'Remote pads only play: game_only devices',
 "Remote pads: the game's hotkeys": "Remote pads: the game's hotkeys on port 1",
 'NETWORK: ACCESS': 'Access levels per paired device: gamepad, stream, full',
}
msg = open(git('rev-parse','--git-path','rebase-merge/message')).read()
lib = None
for k, v in step_subject.items():
    if msg.startswith(k):
        lib = git('log', '--format=%H', '-F', '--grep=' + v, 'origin/main..netcode', cwd=FC).split('\n')[0]
if not lib:
    sys.exit('no mapping for: ' + msg.split('\n')[0])
unmerged = git('diff', '--name-only', '--diff-filter=U').split('\n')
def blocks(text):
    return re.compile(r'<<<<<<< [^\n]*\n(.*?)=======\n(.*?)>>>>>>> [^\n]*\n', re.S)
for f in [u for u in unmerged if u]:
    if f == FC:
        continue
    text = open(f).read()
    if f == '.gitmodules':
        text = blocks(text).sub(lambda m: m.group(1) + ''.join(
            e for e in re.split(r'(?=\[submodule)', m.group(2)) if e and e not in m.group(1)), text)
    elif f.startswith('.github/workflows/'):
        def merge_line(m):
            a, b = m.group(1).rstrip('\n'), m.group(2).rstrip('\n')
            indent = a[:len(a) - len(a.lstrip())]
            tokens = a.split()
            for t in b.split():
                if t not in tokens: tokens.insert(len(tokens) - 1 if tokens[-1] == '\\' else len(tokens), t)
            return indent + ' '.join(tokens) + '\n'
        text = blocks(text).sub(merge_line, text)
    elif f == 'CLAUDE.md':
        text = blocks(text).sub(lambda m: m.group(1), text)
    elif f in ('src/strings.txt', 'lang/es.po'):
        text = blocks(text).sub(lambda m: m.group(1) + m.group(2), text)
    if '<<<<<<<' in text:
        sys.exit('unresolved: ' + f)
    open(f, 'w').write(text)
    git('add', f)
# submodule pointer
subprocess.run(['git', '-C', FC, 'checkout', '-q', lib])
git('add', FC)
left = [u for u in git('diff', '--name-only', '--diff-filter=U').split('\n') if u]
if left:
    sys.exit('still unmerged: ' + ' '.join(left))
print('resolved', msg.split('\n')[0], '-> fc', lib[:7])
