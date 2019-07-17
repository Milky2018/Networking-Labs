package trie;

import java.io.*;
import java.util.Arrays;

public class Trie {
    public static final int BITS = 8;
    public static final int FILLED;
    public static final int POW;

    static {
        POW = 1 << BITS;
        FILLED = POW - 1;
        if (32 % BITS != 0) {
            System.out.println("Bits is illegal");
        }
    }

    public static int parseIpString(String ipString) {
        Integer[] parts = Arrays.stream(ipString.split("\\.")).
                map((s) -> Integer.parseUnsignedInt(s)).
                toArray(Integer[]::new);
        return parts[3] | (parts[2] << 8) | (parts[1] << 16) | (parts[0] << 24);
    }

    public static String parseIpNumber(int ip) {
        String[] nets = new String[4];
        for (int i = 0; i < 4; i++) {
            nets[3 - i] = Integer.toString((ip >> (i * 8)) & 0xff);
        }
        return String.join(".", nets);
    }

    private TrieNode root = new TrieNode(0, 0);

    public static int getBinsByIndex(int i, int index) {
        return (i >> index) & FILLED;
    }

    public void put(String ip, int mask, int port) {
        put(parseIpString(ip), mask, port);
    }

    public void put(int ip, int mask, int port) {
        root.put(ip, mask, port);
    }

    public int get(String ip) {
        return get(parseIpString(ip));
    }

    public int get(int ip) {
        TrieNode find = root.get(ip);
        if (find == null) {
            System.out.println(parseIpNumber(ip) + " not found");
            return 0;
        } else {
            return find.getPort();
        }
    }

    public static void main(String[] args) {

    }

    public long getNodeNumber() {
        return root.counter * 41;
    }

    public void testIp(String ip) {
        System.out.println(get(ip));
    }

    public void testIp(int ip) {
        System.out.println(get(ip));
    }
}

class TrieNode {
    private int ip;
    private int mask;
    private boolean valid = false;
    private int validMask = 0;

    private int port = 0;
    private TrieNode[] sub = new TrieNode[Trie.POW];

    static long counter = 0;

    boolean isValid() {
        return valid;
    }

    TrieNode(int ip, int mask) {
        this.ip = ip;
        if (mask % Trie.BITS == 0 && mask <= 32) {
            this.mask = mask;
        } else {
            System.out.println("illegal mask in: " + Trie.parseIpNumber(ip));
        }
        counter++;
    }

    boolean match(int ip) {
        if (this.mask == 0) {
            return true;
        } else if (this.valid) {
            return this.ip >> (32 - this.validMask) == ip >> (32 - this.validMask);
        } else {
            return this.ip >> (32 - this.mask) == ip >> (32 - this.mask);
        }
    }

    int getMask() {
        return mask;
    }

    TrieNode getSub(int i, boolean autoCreate) {
        if (i > Trie.POW) {
            System.out.println("illegal access to sub");
            return null;
        }
        if (i >= Trie.POW || i < 0) {
            System.out.println("getSub a wrong number: " + i);
            return null;
        }
        if (autoCreate && sub[i] == null) {
            int newMask = this.mask + Trie.BITS;
            sub[i] = new TrieNode(this.ip + (i << (32 - newMask)), newMask);
        }
        return sub[i];
    }

    TrieNode getSub(int i) {
        return getSub(i, true);
    }

    private void addPort(int validMask, int port) {
        if (validMask <= 0 || validMask > 32) {
            System.out.println("add port illegally");
        }
        valid = true;
        this.port = port;
        this.validMask = validMask;
    }

    int getPort() {
        return port;
    }

    void put(int ip, int mask, int port) {
        int from = Trie.getBinsByIndex(ip, 32 - this.mask - Trie.BITS);
        int to = from + (1 << (Trie.BITS - mask + this.mask));
        if (mask > this.mask) {
            if (mask >= this.mask + Trie.BITS) {
                getSub(from).put(ip, mask, port);
            } else {
                for (int i = from; i < to; i++) {
                    getSub(i).put(ip, mask, port);
                }
            }
        } else if (!valid || mask > this.validMask){
            addPort(mask, port);
        }
    }

    TrieNode get(int ip) {
        if (!match(ip)) {
            return null;
        }

        int index = Trie.getBinsByIndex(ip, 32 - this.mask - Trie.BITS);
        if (sub[index] != null && sub[index].get(ip) != null) {
            return sub[index].get(ip);
        } else if (this.valid) {
            return this;
        } else {
            return null;
        }
    }
}