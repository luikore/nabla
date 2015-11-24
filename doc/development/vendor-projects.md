# Vendor projects

Was added in this way:

    git remote add siphash git@github.com:luikore/siphash.git
    git remote add ccut git@github.com:luikore/ccut.git
    git subtree add --prefix=vendor/siphash siphash master --squash
    git subtree add --prefix=vendor/ccut ccut master --squash

To update

    git subtree pull --prefix=vendor/siphash siphash master --squash
    git subtree pull --prefix=vendor/ccut ccut master --squash
