package test;

import trie.Trie;
import subnet.*;

import javax.swing.tree.TreeNode;
import java.io.*;
import java.util.Random;

public class Test {
    private static final int testNumber = 398765;

    public static void main(String[] args) {
        String testFile = "./test-list.txt";
        String config = "./forwarding-table.txt";
        Trie trie = new Trie();
        try {
            makeTestFile(config, testFile);
            makeTrieConfig(trie, config);
            long time = lookUpTest(trie, testFile);

            System.out.println("average time for one lookup: " + time * 1000000 / testNumber + " ns");
        } catch (IOException e) {
            System.out.println("wtf? It is impossible");
        }

        System.out.println("total space: " + trie.getNodeNumber() / 1024 + " KB");
    }

    private static void makeTrieConfig(Trie trie, String filename) throws IOException {
        BufferedReader br = new BufferedReader(new FileReader(filename));
        for (int i = 0; i < testNumber; i++) {
            String[] strings = br.readLine().split(" ");
            int ip = Trie.parseIpString(strings[0]);
            int mask = Integer.parseInt(strings[1]);
            int port = Integer.parseInt(strings[2]);
            trie.put(ip, mask, port);
        }
        br.close();
    }

    private static void makeTestFile(String config, String filename) throws IOException {
        PrintWriter pw = new PrintWriter(new FileWriter(filename));
        BufferedReader br = new BufferedReader(new FileReader(config));

        for (int i = 0; i < testNumber; i++) {
            String[] strings = br.readLine().split(" ");
            int ip = Trie.parseIpString(strings[0]);
            int mask = Integer.parseInt(strings[1]);
            pw.println(Trie.parseIpNumber(makeRandomIp(ip, mask)));
        }

        br.close();
        pw.close();
    }

    private static int makeRandomIp(int ip, int mask) {
        Random random = new Random();
        int filled;
        if (mask == 0) {
            filled = -1;
        } else if (mask == 32) {
            filled = 0;
        } else {
            filled = (1 << (31 - mask)) - 1;
        }
        return ip | (random.nextInt() & filled);
    }

    private static long lookUpTest(Trie trie, String testFile) throws IOException {
        BufferedReader br = new BufferedReader(new FileReader(testFile));
        int[] ip = new int[testNumber];
        int[] port = new int[testNumber];
        for (int i = 0; i < testNumber; i++) {
            ip[i] = Trie.parseIpString(br.readLine());
        }

        long start = System.currentTimeMillis();
        for (int i = 0; i < testNumber; i++) {
            port[i] = trie.get(ip[i]);
        }
        long end = System.currentTimeMillis();

        return end - start;
    }
}
