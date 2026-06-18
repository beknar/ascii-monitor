// Build ascii-monitor on every node in the Jenkins fleet.
//
// The fleet's controller is gemini; the build nodes are abba, calibri, gabriel,
// zariel, taroo (Linux), lulu, jindi, jindi2 (FreeBSD), phoenix (Solaris) and
// hornet (OpenBSD).
// Each node's name is also one of its labels, so `agent { label "${NODE}" }`
// pins one matrix cell per host. Cells run in parallel and are independent
// (failFast is off by default), so a failure on one node doesn't stop the rest.
//
// The build itself is just `gmake` (GNU make exists on every node; on Linux it's
// a symlink to make). The Makefile handles the per-OS specifics (e.g. -lkstat and
// the ncurses include path on Solaris), so every node runs the same command.
// ascii-monitor builds and runs on Linux, FreeBSD, OpenBSD and Solaris. There's nothing to
// run in CI (it's an interactive ncurses TUI with no batch/test mode), so each
// cell builds, archives its binary, and -- only on `main` -- installs that binary
// into /usr/local/bin on the same node it was built on. Because each cell builds
// natively and deploys locally, every host always gets the correct per-OS binary
// with no cross-node copying. Feature/PR builds compile but never deploy.

pipeline {
    agent none

    options {
        timestamps()
        buildDiscarder(logRotator(numToKeepStr: '20'))
    }

    stages {
        stage('Build on the fleet') {
            matrix {
                axes {
                    axis {
                        name 'NODE'
                        values 'abba', 'calibri', 'gabriel', 'zariel', 'taroo',
                               'lulu', 'jindi', 'jindi2', 'phoenix', 'hornet'
                    }
                }

                agent { label "${NODE}" }

                stages {
                    stage('Compile') {
                        steps {
                            echo "Building ascii-monitor on ${NODE} (node=${env.NODE_NAME}, labels=${env.NODE_LABELS})"
                            sh '''
                                set -eu
                                # The Makefile applies the per-OS flags itself (Solaris adds the
                                # ncurses include path and -lkstat), so the command is identical
                                # on every node.
                                gmake clean || true
                                gmake

                                ls -l ascii-monitor
                                # Give each node's binary a distinct name so the archived
                                # artifacts from all cells don't collide.
                                cp ascii-monitor "ascii-monitor-${NODE_NAME}"
                            '''
                        }
                    }

                    stage('Deploy to /usr/local/bin') {
                        // Only the mainline gets installed onto the fleet; PR and
                        // feature builds compile and archive but must not touch
                        // the binary that's live on each host.
                        when { branch 'main' }
                        steps {
                            echo "Installing ascii-monitor to /usr/local/bin on ${NODE} (node=${env.NODE_NAME})"
                            sh '''
                                set -eu
                                # The agent runs as the node's connecting user: storm
                                # (passwordless sudo) on most hosts, but root on the
                                # FreeBSD jails (jindi, jindi2) where sudo may be absent.
                                # So escalate with sudo only when we're not already root.
                                if [ "$(id -u)" -eq 0 ]; then SUDO=""; else SUDO="sudo -n"; fi

                                # Portable install: cp + chmod + atomic mv. Avoids the
                                # GNU/Solaris `install(1)` syntax divergence, and the mv
                                # swaps the file in place so a running copy isn't clobbered
                                # ("text file busy") mid-write.
                                $SUDO mkdir -p /usr/local/bin
                                $SUDO cp ascii-monitor /usr/local/bin/ascii-monitor.new
                                $SUDO chmod 0755 /usr/local/bin/ascii-monitor.new
                                $SUDO mv /usr/local/bin/ascii-monitor.new /usr/local/bin/ascii-monitor

                                echo "Deployed:"
                                ls -l /usr/local/bin/ascii-monitor
                            '''
                        }
                    }
                }

                post {
                    success {
                        archiveArtifacts artifacts: "ascii-monitor-${env.NODE_NAME}", fingerprint: true
                    }
                }
            }
        }
    }
}
