# btc-opreturns

`btc-opreturns` is a simple CLI tool that works backwards down the blockchain,
searching for transactions with OP_RETURNs larger than 80 bytes. When found,
the OP_RETURN data is dumped into a text file in HEX format with the transaction
ID as the name of the file.

It uses `bitcoin-cli` to gather block and transaction information. This is kind of
slow as the bitcoin-cli process is executed for every querry.

## Requirements

* `bitcoin-cli` (must be in `$PATH` and connected to a full node)
* C++20 compiler (Clang/GCC)
* Boost libraries (JSON)

## Build

```sh
mkdir build && cd build
cmake ..
make
```

## Usage

```sh
./btc-opreturns [blockheight] [maxblocks]
```

* `blockheight` – optional; Blockheight to start searching. Defaults to current tip.
* `maxblocks` - optional; Maximum number of blocks to process. Defaults to all of them.

### Example

```sh
./btc-opreturns 922251 1000
./btc-opreturns 0 10
```
The second example will check the ten blocks from the tip.

## License

MIT

