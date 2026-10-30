// Visualiser for one monument group: draws the world geometry the search numbers refer to.
//
// Everything shown is re-derived here with plain integer math (independent of the solver):
//   - 58x58 spawning box per monument anchored at [center-29, center+28]
//   - covered = box block within DISK_R of at least one drawn AFK point (union view)
//   - 257x257 single-gate window centred on the given gate centre
//   - chunk (16) / 128 / region (512) grid lines, aligned to world origin
//
// Build & run (from the repository root):
//   javac -d build/tools tools/DrawGroup.java
//   java -cp build/tools tools.DrawGroup outPrefix scale x1 z1 x2 z2 sx sz gateX gateZ m1x m1z [m2x m2z ...]
// Example (seed 40, triple with full dual coverage):
//   java -cp build/tools tools.DrawGroup tools/seed40 2 \
//        -100948 101820 -100938 101867 -100940 101842 -100928 101832 \
//        -101008 101920 -100960 101744 -100848 101888
// Outputs <outPrefix>-dual.png (two dummies + union area) and <outPrefix>-single.png (one dummy).
package tools;

import javax.imageio.ImageIO;
import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Font;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class DrawGroup {

    static final int BOX_LO = -29, BOX_HI = 28;   // 58-wide box anchored at [c-29, c+28]
    static final int R = 128, R2 = R * R;         // AFK keep-alive disk
    static final int WIN = 257;                   // single-gate footprint window

    public static void main(String[] args) throws Exception {
        if (args.length < 13) {
            System.err.println("usage: DrawGroup <outPrefix> <scale> <x1> <z1> <x2> <z2> <sx> <sz> "
                    + "<gateX> <gateZ> <m1x> <m1z> [m2x m2z ...]");
            System.exit(1);
        }
        int a = 0;
        String prefix = args[a++];
        int scale = Integer.parseInt(args[a++]);
        int d1x = Integer.parseInt(args[a++]), d1z = Integer.parseInt(args[a++]);
        int d2x = Integer.parseInt(args[a++]), d2z = Integer.parseInt(args[a++]);
        int sx = Integer.parseInt(args[a++]), sz = Integer.parseInt(args[a++]);
        int gx = Integer.parseInt(args[a++]), gz = Integer.parseInt(args[a++]);
        List<int[]> mons = new ArrayList<>();
        while (a + 1 < args.length + 1 && a + 1 <= args.length - 1 + 1 && a < args.length) {
            mons.add(new int[]{Integer.parseInt(args[a]), Integer.parseInt(args[a + 1])});
            a += 2;
        }

        render(prefix + "-dual.png", scale, mons,
                new int[][]{{d1x, d1z}, {d2x, d2z}}, new Color(0x00, 0xC8, 0xFF),
                new int[]{gx, gz}, "dual");
        render(prefix + "-single.png", scale, mons,
                new int[][]{{sx, sz}}, new Color(0xFF, 0xD0, 0x00),
                new int[]{gx, gz}, "single");
        System.out.println("written: " + prefix + "-dual.png, " + prefix + "-single.png");
    }

    static void render(String file, int scale, List<int[]> mons, int[][] pts, Color ptColor,
                       int[] gate, String tag) throws Exception {
        // world extent: boxes + disks + gate window + margin
        int mnx = Integer.MAX_VALUE, mxx = Integer.MIN_VALUE;
        int mnz = Integer.MAX_VALUE, mxz = Integer.MIN_VALUE;
        for (int[] m : mons) {
            mnx = Math.min(mnx, m[0] + BOX_LO);
            mxx = Math.max(mxx, m[0] + BOX_HI);
            mnz = Math.min(mnz, m[1] + BOX_LO);
            mxz = Math.max(mxz, m[1] + BOX_HI);
        }
        for (int[] p : pts) {
            mnx = Math.min(mnx, p[0] - R);
            mxx = Math.max(mxx, p[0] + R);
            mnz = Math.min(mnz, p[1] - R);
            mxz = Math.max(mxz, p[1] + R);
        }
        mnx = Math.min(mnx, gate[0] - WIN / 2) - 24;
        mxx = Math.max(mxx, gate[0] + WIN / 2) + 24;
        mnz = Math.min(mnz, gate[1] - WIN / 2) - 40;
        mxz = Math.max(mxz, gate[1] + WIN / 2) + 24;
        int w = mxx - mnx + 1, h = mxz - mnz + 1;

        // coverage per world block (union over points), integer math
        boolean[][] covered = new boolean[h][w];
        int count = 0;
        for (int[] m : mons) {
            for (int z = m[1] + BOX_LO; z <= m[1] + BOX_HI; z++) {
                for (int x = m[0] + BOX_LO; x <= m[0] + BOX_HI; x++) {
                    boolean cov = false;
                    for (int[] p : pts) {
                        long dx = (long) x - p[0], dz = (long) z - p[1];
                        if (dx * dx + dz * dz <= (long) R2) {
                            cov = true;
                            break;
                        }
                    }
                    int pz = z - mnz, px = x - mnx;
                    if (!covered[pz][px] && cov)
                        count++;
                    covered[pz][px] |= cov;
                }
            }
        }

        BufferedImage img = new BufferedImage(w * scale, h * scale, BufferedImage.TYPE_INT_RGB);
        Graphics2D g = img.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setColor(new Color(0x10, 0x14, 0x1A));
        g.fillRect(0, 0, img.getWidth(), img.getHeight());

        // covered / uncovered spawning blocks
        for (int[] m : mons) {
            for (int z = m[1] + BOX_LO; z <= m[1] + BOX_HI; z++)
                for (int x = m[0] + BOX_LO; x <= m[0] + BOX_HI; x++) {
                    boolean cov = covered[z - mnz][x - mnx];
                    g.setColor(cov ? new Color(0x2E, 0x8B, 0x57) : new Color(0x3A, 0x3A, 0x44));
                    g.fillRect((x - mnx) * scale, (z - mnz) * scale, scale, scale);
                }
        }

        // grid: chunk (16) faint, 128 medium, region (512) strong - aligned to world origin
        for (int x = (int) Math.ceil(mnx / 16.0) * 16; x <= mxx; x += 16) {
            g.setColor(x % 512 == 0 ? new Color(0x88, 0x88, 0xEE) : x % 128 == 0
                    ? new Color(0x55, 0x55, 0x77) : new Color(0x2A, 0x2E, 0x38));
            g.fillRect((x - mnx) * scale, 0, Math.max(1, scale / 8), img.getHeight());
        }
        for (int z = (int) Math.ceil(mnz / 16.0) * 16; z <= mxz; z += 16) {
            g.setColor(z % 512 == 0 ? new Color(0x88, 0x88, 0xEE) : z % 128 == 0
                    ? new Color(0x55, 0x55, 0x77) : new Color(0x2A, 0x2E, 0x38));
            g.fillRect(0, (z - mnz) * scale, img.getWidth(), Math.max(1, scale / 8));
        }

        // 128-disk outlines
        g.setStroke(new BasicStroke(2f));
        g.setColor(ptColor);
        for (int[] p : pts)
            g.drawOval((p[0] - R - mnx) * scale, (p[1] - R - mnz) * scale, 2 * R * scale, 2 * R * scale);

        // single-gate 257x257 window
        g.setColor(new Color(0xFF, 0x8C, 0x00));
        g.setStroke(new BasicStroke(2f, BasicStroke.CAP_BUTT, BasicStroke.JOIN_BEVEL,
                0, new float[]{8f, 6f}, 0));
        g.drawRect((gate[0] - WIN / 2 - mnx) * scale, (gate[1] - WIN / 2 - mnz) * scale,
                (WIN - 1) * scale, (WIN - 1) * scale);
        g.setStroke(new BasicStroke(1f));

        // monument boxes (thin outline) and centres
        g.setColor(new Color(0xE8, 0xE8, 0xE8));
        for (int[] m : mons)
            g.drawRect((m[0] + BOX_LO - mnx) * scale, (m[1] + BOX_LO - mnz) * scale,
                    58 * scale, 58 * scale);
        Font f = new Font(Font.MONOSPACED, Font.BOLD, 13);
        g.setFont(f);
        int mi = 1;
        for (int[] m : mons) {
            int px = (m[0] - mnx) * scale, py = (m[1] - mnz) * scale;
            g.setColor(new Color(0xFF, 0x40, 0x40));
            g.fillRect(px - 7, py - 1, 15, 3);
            g.fillRect(px - 1, py - 7, 3, 15);
            g.setColor(Color.WHITE);
            g.drawString("M" + mi + " (" + m[0] + "," + m[1] + ")", px + 8, py - 8);
            mi++;
        }

        // AFK points
        String[] names = {"d1", "d2"};
        for (int i = 0; i < pts.length; i++) {
            int px = (pts[i][0] - mnx) * scale, py = (pts[i][1] - mnz) * scale;
            g.setColor(ptColor);
            g.fillRect(px - 2, py - 2, 5, 5);
            g.setColor(Color.WHITE);
            g.drawString((pts.length > 1 ? names[i] : "single") + " (" + pts[i][0] + "," + pts[i][1] + ")",
                    px + 8, py + 16);
        }
        g.setColor(new Color(0xFF, 0x8C, 0x00));
        g.drawString("gate (" + gate[0] + "," + gate[1] + ")",
                (gate[0] - mnx) * scale + 8, (gate[1] - mnz) * scale + 6);

        // axis tick labels along the top (x) and left (z)
        g.setColor(new Color(0x9A, 0x9A, 0x9A));
        Font small = new Font(Font.MONOSPACED, Font.PLAIN, 11);
        g.setFont(small);
        for (int x = (int) Math.ceil(mnx / 128.0) * 128; x <= mxx; x += 128)
            g.drawString(String.valueOf(x), (x - mnx) * scale + 2, 12);
        for (int z = (int) Math.ceil(mnz / 128.0) * 128; z <= mxz; z += 128)
            g.drawString(String.valueOf(z), 2, (z - mnz) * scale + 14);

        // header
        g.setColor(Color.WHITE);
        g.setFont(f);
        g.drawString(tag + ": covered " + count + " / " + mons.size() + "x" + (BOX_HI - BOX_LO + 1) * (BOX_HI - BOX_LO + 1)
                + " blocks (green=covered, grey=uncovered)", 8, img.getHeight() - 8);

        ImageIO.write(img, "png", new File(file));
        g.dispose();
    }
}
