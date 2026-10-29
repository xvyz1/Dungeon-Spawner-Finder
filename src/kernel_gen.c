#include "kernel.h"

double root0(const World* w, const double* in2d, int qx0, int qz0, int x, int y, int z) {
    double m[1]; unsigned char h[1]; memset(h, 0, sizeof h);
    int o2 = ((z >> 2) - qz0) * 5 + ((x >> 2) - qx0);
    double t0 = clamped_map((double) y, -64.0, -40.0, 0.0000000000000000, 1.0000000000000000);
    double t1;
    if (t0 == 0.0) t1 = 0.0; else {
        double t2 = t0;
    double t3 = clamped_map((double) y, 240.0, 256.0, 1.0000000000000000, 0.0000000000000000);
    double t4;
    if (t3 == 0.0) t4 = 0.0; else {
        double t5 = t3;
    if (!h[0]) {
    double t6;
    if ((4.0000000000000000) == 0.0) t6 = 0.0; else {
        double t7 = (4.0000000000000000);
    double t8 = clamped_map((double) y, -64.0, 320.0, 1.5000000000000000, -1.5000000000000000);
    double t9 = t8 + in2d[0 * 25 + o2];
    double t10;
    if (in2d[1 * 25 + o2] == 0.0) t10 = 0.0; else {
        double t11 = in2d[1 * 25 + o2];
    double t12 = noise_normal(w, 0, x * 1500.0000000000000, y * 0.0000000000000000, z * 1500.0000000000000);
    double t13 = t12;
    double t14 = t13 > 0.0 ? t13 : t13 * 0.5;
        t10 = t11 * t14;
    }
    double t15 = t9 + t10;
    double t16;
    if (t15 == 0.0) t16 = 0.0; else {
        double t17 = t15;
        t16 = t17 * in2d[2 * 25 + o2];
    }
    double t18 = t16;
    double t19 = t18 > 0.0 ? t18 : t18 * 0.25;
        t6 = t7 * t19;
    }
    double t20 = blended(w, x, y, z);
    double t21 = t6 + t20;
        m[0] = t21; h[0] = 1;
    }
    double t22 = m[0];
    double t23;
    if (t22 >= -1000000.0000000000 && t22 < 1.5625000000000000) {
    if (!h[0]) {
    double t24;
    if ((4.0000000000000000) == 0.0) t24 = 0.0; else {
        double t25 = (4.0000000000000000);
    double t26 = clamped_map((double) y, -64.0, 320.0, 1.5000000000000000, -1.5000000000000000);
    double t27 = t26 + in2d[0 * 25 + o2];
    double t28;
    if (in2d[1 * 25 + o2] == 0.0) t28 = 0.0; else {
        double t29 = in2d[1 * 25 + o2];
    double t30 = noise_normal(w, 0, x * 1500.0000000000000, y * 0.0000000000000000, z * 1500.0000000000000);
    double t31 = t30;
    double t32 = t31 > 0.0 ? t31 : t31 * 0.5;
        t28 = t29 * t32;
    }
    double t33 = t27 + t28;
    double t34;
    if (t33 == 0.0) t34 = 0.0; else {
        double t35 = t33;
        t34 = t35 * in2d[2 * 25 + o2];
    }
    double t36 = t34;
    double t37 = t36 > 0.0 ? t36 : t36 * 0.25;
        t24 = t25 * t37;
    }
    double t38 = blended(w, x, y, z);
    double t39 = t24 + t38;
        m[0] = t39; h[0] = 1;
    }
    double t40 = m[0];
    double t41;
    double t42;
    if ((5.0000000000000000) == 0.0) t42 = 0.0; else {
        double t43 = (5.0000000000000000);
    double t44 = noise_normal(w, 1, x * 0.75000000000000000, y * 0.50000000000000000, z * 0.75000000000000000);
    double t45 = (0.37000000000000000) + t44;
    double t46 = clamped_map((double) y, -10.0, 30.0, 0.30000000000000000, 0.0000000000000000);
    double t47 = t45 + t46;
    double t48 = t47;
    double t49;
    double t50;
    if ((-0.050000000000000000) == 0.0) t50 = 0.0; else {
        double t51 = (-0.050000000000000000);
    double t52 = noise_normal(w, 2, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t50 = t51 * t52;
    }
    double t53 = (-0.050000000000000000) + t50;
    double t54;
    if (t53 == 0.0) t54 = 0.0; else {
        double t55 = t53;
    double t56 = noise_normal(w, 3, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
    double t57 = t56;
    double t58 = fabs(t57);
    double t59 = (-0.40000000000000000) + t58;
        t54 = t55 * t59;
    }
    double t60 = noise_normal(w, 5, x * 2.0000000000000000, y * 1.0000000000000000, z * 2.0000000000000000);
    double t61 = t60;
    double t62 = rarity3d(t61);
    double t63 = t62 * fabs(noise_normal(w, 4, x / t62, y / t62, z / t62));
    double t64 = t63;
    double t65 = noise_normal(w, 5, x * 2.0000000000000000, y * 1.0000000000000000, z * 2.0000000000000000);
    double t66 = t65;
    double t67 = rarity3d(t66);
    double t68 = t67 * fabs(noise_normal(w, 6, x / t67, y / t67, z / t67));
    double t69 = jmax(t64, t68);
    double t70;
    if ((-0.011499999999999996) == 0.0) t70 = 0.0; else {
        double t71 = (-0.011499999999999996);
    double t72 = noise_normal(w, 7, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t70 = t71 * t72;
    }
    double t73 = (-0.076500000000000000) + t70;
    double t74 = t69 + t73;
    double t75 = jclamp(t74, -1.0000000000000000, 1.0000000000000000);
    double t76 = t54 + t75;
    t49 = jmin(t48, t76);
        t42 = t43 * t49;
    }
    t41 = jmin(t40, t42);
        t23 = t41;
    } else {
    double t77;
    if ((4.0000000000000000) == 0.0) t77 = 0.0; else {
        double t78 = (4.0000000000000000);
    double t79 = noise_normal(w, 8, x * 1.0000000000000000, y * 8.0000000000000000, z * 1.0000000000000000);
    double t80 = t79;
    double t81 = t80 * t80;
        t77 = t78 * t81;
    }
    double t82 = noise_normal(w, 9, x * 1.0000000000000000, y * 0.66666666666666660, z * 1.0000000000000000);
    double t83 = (0.27000000000000000) + t82;
    double t84 = jclamp(t83, -1.0000000000000000, 1.0000000000000000);
    double t85;
    if ((-0.64000000000000000) == 0.0) t85 = 0.0; else {
        double t86 = (-0.64000000000000000);
    if (!h[0]) {
    double t87;
    if ((4.0000000000000000) == 0.0) t87 = 0.0; else {
        double t88 = (4.0000000000000000);
    double t89 = clamped_map((double) y, -64.0, 320.0, 1.5000000000000000, -1.5000000000000000);
    double t90 = t89 + in2d[0 * 25 + o2];
    double t91;
    if (in2d[1 * 25 + o2] == 0.0) t91 = 0.0; else {
        double t92 = in2d[1 * 25 + o2];
    double t93 = noise_normal(w, 0, x * 1500.0000000000000, y * 0.0000000000000000, z * 1500.0000000000000);
    double t94 = t93;
    double t95 = t94 > 0.0 ? t94 : t94 * 0.5;
        t91 = t92 * t95;
    }
    double t96 = t90 + t91;
    double t97;
    if (t96 == 0.0) t97 = 0.0; else {
        double t98 = t96;
        t97 = t98 * in2d[2 * 25 + o2];
    }
    double t99 = t97;
    double t100 = t99 > 0.0 ? t99 : t99 * 0.25;
        t87 = t88 * t100;
    }
    double t101 = blended(w, x, y, z);
    double t102 = t87 + t101;
        m[0] = t102; h[0] = 1;
    }
        t85 = t86 * m[0];
    }
    double t103 = (1.5000000000000000) + t85;
    double t104 = jclamp(t103, 0.0000000000000000, 0.50000000000000000);
    double t105 = t84 + t104;
    double t106 = t77 + t105;
    double t107 = t106;
    double t108;
    double t109 = noise_normal(w, 1, x * 0.75000000000000000, y * 0.50000000000000000, z * 0.75000000000000000);
    double t110 = (0.37000000000000000) + t109;
    double t111 = clamped_map((double) y, -10.0, 30.0, 0.30000000000000000, 0.0000000000000000);
    double t112 = t110 + t111;
    double t113 = t112;
    double t114;
    double t115;
    if ((-0.050000000000000000) == 0.0) t115 = 0.0; else {
        double t116 = (-0.050000000000000000);
    double t117 = noise_normal(w, 2, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t115 = t116 * t117;
    }
    double t118 = (-0.050000000000000000) + t115;
    double t119;
    if (t118 == 0.0) t119 = 0.0; else {
        double t120 = t118;
    double t121 = noise_normal(w, 3, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
    double t122 = t121;
    double t123 = fabs(t122);
    double t124 = (-0.40000000000000000) + t123;
        t119 = t120 * t124;
    }
    double t125 = noise_normal(w, 5, x * 2.0000000000000000, y * 1.0000000000000000, z * 2.0000000000000000);
    double t126 = t125;
    double t127 = rarity3d(t126);
    double t128 = t127 * fabs(noise_normal(w, 4, x / t127, y / t127, z / t127));
    double t129 = t128;
    double t130 = noise_normal(w, 5, x * 2.0000000000000000, y * 1.0000000000000000, z * 2.0000000000000000);
    double t131 = t130;
    double t132 = rarity3d(t131);
    double t133 = t132 * fabs(noise_normal(w, 6, x / t132, y / t132, z / t132));
    double t134 = jmax(t129, t133);
    double t135;
    if ((-0.011499999999999996) == 0.0) t135 = 0.0; else {
        double t136 = (-0.011499999999999996);
    double t137 = noise_normal(w, 7, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t135 = t136 * t137;
    }
    double t138 = (-0.076500000000000000) + t135;
    double t139 = t134 + t138;
    double t140 = jclamp(t139, -1.0000000000000000, 1.0000000000000000);
    double t141 = t119 + t140;
    t114 = jmin(t113, t141);
    t108 = jmin(t107, t114);
    double t142 = t108;
    double t143;
    double t144 = noise_normal(w, 11, x * 2.0000000000000000, y * 1.0000000000000000, z * 2.0000000000000000);
    double t145 = t144;
    double t146 = rarity2d(t145);
    double t147 = t146 * fabs(noise_normal(w, 10, x / t146, y / t146, z / t146));
    double t148;
    if ((0.083000000000000000) == 0.0) t148 = 0.0; else {
        double t149 = (0.083000000000000000);
    double t150;
    if ((-0.35000000000000003) == 0.0) t150 = 0.0; else {
        double t151 = (-0.35000000000000003);
    double t152 = noise_normal(w, 12, x * 2.0000000000000000, y * 1.0000000000000000, z * 2.0000000000000000);
        t150 = t151 * t152;
    }
    double t153 = (-0.95000000000000000) + t150;
        t148 = t149 * t153;
    }
    double t154 = t147 + t148;
    double t155 = t154;
    double t156;
    if ((8.0000000000000000) == 0.0) t156 = 0.0; else {
        double t157 = (8.0000000000000000);
    double t158 = noise_normal(w, 13, x * 1.0000000000000000, y * 0.0000000000000000, z * 1.0000000000000000);
        t156 = t157 * t158;
    }
    double t159 = (0.0000000000000000) + t156;
    double t160 = clamped_map((double) y, -64.0, 320.0, 8.0000000000000000, -40.000000000000000);
    double t161 = t159 + t160;
    double t162 = t161;
    double t163 = fabs(t162);
    double t164;
    if ((-0.35000000000000003) == 0.0) t164 = 0.0; else {
        double t165 = (-0.35000000000000003);
    double t166 = noise_normal(w, 12, x * 2.0000000000000000, y * 1.0000000000000000, z * 2.0000000000000000);
        t164 = t165 * t166;
    }
    double t167 = (-0.95000000000000000) + t164;
    double t168 = t163 + t167;
    double t169 = t168;
    double t170 = t169 * t169 * t169;
    double t171 = jmax(t155, t170);
    double t172 = jclamp(t171, -1.0000000000000000, 1.0000000000000000);
    double t173;
    if ((-0.050000000000000000) == 0.0) t173 = 0.0; else {
        double t174 = (-0.050000000000000000);
    double t175 = noise_normal(w, 2, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t173 = t174 * t175;
    }
    double t176 = (-0.050000000000000000) + t173;
    double t177;
    if (t176 == 0.0) t177 = 0.0; else {
        double t178 = t176;
    double t179 = noise_normal(w, 3, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
    double t180 = t179;
    double t181 = fabs(t180);
    double t182 = (-0.40000000000000000) + t181;
        t177 = t178 * t182;
    }
    double t183 = t172 + t177;
    t143 = jmin(t142, t183);
    double t184 = t143;
    double t185;
    if ((2.0000000000000000) == 0.0) t185 = 0.0; else {
        double t186 = (2.0000000000000000);
    double t187 = noise_normal(w, 14, x * 25.000000000000000, y * 0.30000000000000000, z * 25.000000000000000);
        t185 = t186 * t187;
    }
    double t188;
    if ((-1.0000000000000000) == 0.0) t188 = 0.0; else {
        double t189 = (-1.0000000000000000);
    double t190 = noise_normal(w, 15, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t188 = t189 * t190;
    }
    double t191 = (-1.0000000000000000) + t188;
    double t192 = t185 + t191;
    double t193;
    if (t192 == 0.0) t193 = 0.0; else {
        double t194 = t192;
    double t195;
    if ((0.55000000000000000) == 0.0) t195 = 0.0; else {
        double t196 = (0.55000000000000000);
    double t197 = noise_normal(w, 16, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t195 = t196 * t197;
    }
    double t198 = (0.55000000000000000) + t195;
    double t199 = t198;
    double t200 = t199 * t199 * t199;
        t193 = t194 * t200;
    }
    double t201 = t193;
    double t202;
    if (t201 >= -1000000.0000000000 && t201 < 0.030000000000000000) {
        t202 = (-1000000.0000000000);
    } else {
    double t203;
    if ((2.0000000000000000) == 0.0) t203 = 0.0; else {
        double t204 = (2.0000000000000000);
    double t205 = noise_normal(w, 14, x * 25.000000000000000, y * 0.30000000000000000, z * 25.000000000000000);
        t203 = t204 * t205;
    }
    double t206;
    if ((-1.0000000000000000) == 0.0) t206 = 0.0; else {
        double t207 = (-1.0000000000000000);
    double t208 = noise_normal(w, 15, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t206 = t207 * t208;
    }
    double t209 = (-1.0000000000000000) + t206;
    double t210 = t203 + t209;
    double t211;
    if (t210 == 0.0) t211 = 0.0; else {
        double t212 = t210;
    double t213;
    if ((0.55000000000000000) == 0.0) t213 = 0.0; else {
        double t214 = (0.55000000000000000);
    double t215 = noise_normal(w, 16, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t213 = t214 * t215;
    }
    double t216 = (0.55000000000000000) + t213;
    double t217 = t216;
    double t218 = t217 * t217 * t217;
        t211 = t212 * t218;
    }
        t202 = t211;
    }
    double t219 = jmax(t184, t202);
        t23 = t219;
    }
    double t220 = (0.078125000000000000) + t23;
        t4 = t5 * t220;
    }
    double t221 = (-0.078125000000000000) + t4;
    double t222 = (-0.11718750000000000) + t221;
        t1 = t2 * t222;
    }
    double t223 = (0.11718750000000000) + t1;
    return t223;
}

double root1(const World* w, const double* in2d, int qx0, int qz0, int x, int y, int z) {
    double m[1]; unsigned char h[1]; memset(h, 0, sizeof h);
    int o2 = ((z >> 2) - qz0) * 5 + ((x >> 2) - qx0); (void) m; (void) o2; (void) in2d;
    double t0 = clamped_map((double) y, -4064.0, 4062.0, -4064.0000000000000, 4062.0000000000000);
    double t1 = t0;
    double t2;
    if (t1 >= -60.000000000000000 && t1 < 321.00000000000000) {
    double t3 = noise_normal(w, 17, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t2 = t3;
    } else {
        t2 = (-1.0000000000000000);
    }
    return t2;
}

double root2(const World* w, const double* in2d, int qx0, int qz0, int x, int y, int z) {
    double m[1]; unsigned char h[1]; memset(h, 0, sizeof h);
    int o2 = ((z >> 2) - qz0) * 5 + ((x >> 2) - qx0); (void) m; (void) o2; (void) in2d;
    double t0 = clamped_map((double) y, -4064.0, 4062.0, -4064.0000000000000, 4062.0000000000000);
    double t1 = t0;
    double t2;
    if (t1 >= -60.000000000000000 && t1 < 321.00000000000000) {
    double t3;
    if ((-0.025000000000000000) == 0.0) t3 = 0.0; else {
        double t4 = (-0.025000000000000000);
    double t5 = noise_normal(w, 18, x * 1.0000000000000000, y * 1.0000000000000000, z * 1.0000000000000000);
        t3 = t4 * t5;
    }
    double t6 = (-0.075000000000000010) + t3;
        t2 = t6;
    } else {
        t2 = (0.0000000000000000);
    }
    return t2;
}

double root3(const World* w, const double* in2d, int qx0, int qz0, int x, int y, int z) {
    double m[1]; unsigned char h[1]; memset(h, 0, sizeof h);
    int o2 = ((z >> 2) - qz0) * 5 + ((x >> 2) - qx0); (void) m; (void) o2; (void) in2d;
    double t0 = clamped_map((double) y, -4064.0, 4062.0, -4064.0000000000000, 4062.0000000000000);
    double t1 = t0;
    double t2;
    if (t1 >= -60.000000000000000 && t1 < 321.00000000000000) {
    double t3 = noise_normal(w, 19, x * 2.6666666666666665, y * 2.6666666666666665, z * 2.6666666666666665);
        t2 = t3;
    } else {
        t2 = (0.0000000000000000);
    }
    return t2;
}

double root4(const World* w, const double* in2d, int qx0, int qz0, int x, int y, int z) {
    double m[1]; unsigned char h[1]; memset(h, 0, sizeof h);
    int o2 = ((z >> 2) - qz0) * 5 + ((x >> 2) - qx0); (void) m; (void) o2; (void) in2d;
    double t0 = clamped_map((double) y, -4064.0, 4062.0, -4064.0000000000000, 4062.0000000000000);
    double t1 = t0;
    double t2;
    if (t1 >= -60.000000000000000 && t1 < 321.00000000000000) {
    double t3 = noise_normal(w, 20, x * 2.6666666666666665, y * 2.6666666666666665, z * 2.6666666666666665);
        t2 = t3;
    } else {
        t2 = (0.0000000000000000);
    }
    return t2;
}

double eval_root(const World* w, int k, const double* in2d, int qx0, int qz0, int x, int y, int z) {
    switch (k) {
        case 0: return root0(w, in2d, qx0, qz0, x, y, z);
        case 1: return root1(w, in2d, qx0, qz0, x, y, z);
        case 2: return root2(w, in2d, qx0, qz0, x, y, z);
        case 3: return root3(w, in2d, qx0, qz0, x, y, z);
        case 4: return root4(w, in2d, qx0, qz0, x, y, z);
        default: return 0.0;
    }
}
