BASEPATH=$( cd -- "$( dirname -- "${BASH_SOURCE[0]:-$0}" )" &> /dev/null && pwd )
ROOTDIR="$BASEPATH"
PROGRAM=$(basename "${BASH_SOURCE[0]:-$0}")

whoami
ls
cd docs/build_docs/guide/script
chmod +x ./install_iSulad_on_Ubuntu_20_04_LTS.sh
./install_iSulad_on_Ubuntu_20_04_LTS.sh
