.global idt_flush
idt_flush:
    lidt (%rdi)
    ret

isr_stub_0:
    pushq $0
    pushq $0
    jmp isr_common_stub

isr_stub_1:
    pushq $0
    pushq $1
    jmp isr_common_stub

isr_stub_2:
    pushq $0
    pushq $2
    jmp isr_common_stub

isr_stub_3:
    pushq $0
    pushq $3
    jmp isr_common_stub

isr_stub_4:
    pushq $0
    pushq $4
    jmp isr_common_stub

isr_stub_5:
    pushq $0
    pushq $5
    jmp isr_common_stub

isr_stub_6:
    pushq $0
    pushq $6
    jmp isr_common_stub

isr_stub_7:
    pushq $0
    pushq $7
    jmp isr_common_stub

isr_stub_8:
    pushq $8
    jmp isr_common_stub

isr_stub_9:
    pushq $0
    pushq $9
    jmp isr_common_stub

isr_stub_10:
    pushq $10
    jmp isr_common_stub

isr_stub_11:
    pushq $11
    jmp isr_common_stub

isr_stub_12:
    pushq $12
    jmp isr_common_stub

isr_stub_13:
    pushq $13
    jmp isr_common_stub

isr_stub_14:
    pushq $14
    jmp isr_common_stub

isr_stub_15:
    pushq $0
    pushq $15
    jmp isr_common_stub

isr_stub_16:
    pushq $0
    pushq $16
    jmp isr_common_stub

isr_stub_17:
    pushq $17
    jmp isr_common_stub

isr_stub_18:
    pushq $0
    pushq $18
    jmp isr_common_stub

isr_stub_19:
    pushq $0
    pushq $19
    jmp isr_common_stub

isr_stub_20:
    pushq $0
    pushq $20
    jmp isr_common_stub

isr_stub_21:
    pushq $0
    pushq $21
    jmp isr_common_stub

isr_stub_22:
    pushq $0
    pushq $22
    jmp isr_common_stub

isr_stub_23:
    pushq $0
    pushq $23
    jmp isr_common_stub

isr_stub_24:
    pushq $0
    pushq $24
    jmp isr_common_stub

isr_stub_25:
    pushq $0
    pushq $25
    jmp isr_common_stub

isr_stub_26:
    pushq $0
    pushq $26
    jmp isr_common_stub

isr_stub_27:
    pushq $0
    pushq $27
    jmp isr_common_stub

isr_stub_28:
    pushq $0
    pushq $28
    jmp isr_common_stub

isr_stub_29:
    pushq $0
    pushq $29
    jmp isr_common_stub

isr_stub_30:
    pushq $30
    jmp isr_common_stub

isr_stub_31:
    pushq $0
    pushq $31
    jmp isr_common_stub

isr_stub_32:
    pushq $0
    pushq $32
    jmp isr_common_stub

isr_stub_33:
    pushq $0
    pushq $33
    jmp isr_common_stub

isr_stub_34:
    pushq $0
    pushq $34
    jmp isr_common_stub

isr_stub_35:
    pushq $0
    pushq $35
    jmp isr_common_stub

isr_stub_36:
    pushq $0
    pushq $36
    jmp isr_common_stub

isr_stub_37:
    pushq $0
    pushq $37
    jmp isr_common_stub

isr_stub_38:
    pushq $0
    pushq $38
    jmp isr_common_stub

isr_stub_39:
    pushq $0
    pushq $39
    jmp isr_common_stub

isr_stub_40:
    pushq $0
    pushq $40
    jmp isr_common_stub

isr_stub_41:
    pushq $0
    pushq $41
    jmp isr_common_stub

isr_stub_42:
    pushq $0
    pushq $42
    jmp isr_common_stub

isr_stub_43:
    pushq $0
    pushq $43
    jmp isr_common_stub

isr_stub_44:
    pushq $0
    pushq $44
    jmp isr_common_stub

isr_stub_45:
    pushq $0
    pushq $45
    jmp isr_common_stub

isr_stub_46:
    pushq $0
    pushq $46
    jmp isr_common_stub

isr_stub_47:
    pushq $0
    pushq $47
    jmp isr_common_stub

isr_stub_48:
    pushq $0
    pushq $48
    jmp isr_common_stub

isr_stub_49:
    pushq $0
    pushq $49
    jmp isr_common_stub

isr_stub_50:
    pushq $0
    pushq $50
    jmp isr_common_stub

isr_stub_51:
    pushq $0
    pushq $51
    jmp isr_common_stub

isr_stub_52:
    pushq $0
    pushq $52
    jmp isr_common_stub

isr_stub_53:
    pushq $0
    pushq $53
    jmp isr_common_stub

isr_stub_54:
    pushq $0
    pushq $54
    jmp isr_common_stub

isr_stub_55:
    pushq $0
    pushq $55
    jmp isr_common_stub

isr_stub_56:
    pushq $0
    pushq $56
    jmp isr_common_stub

isr_stub_57:
    pushq $0
    pushq $57
    jmp isr_common_stub

isr_stub_58:
    pushq $0
    pushq $58
    jmp isr_common_stub

isr_stub_59:
    pushq $0
    pushq $59
    jmp isr_common_stub

isr_stub_60:
    pushq $0
    pushq $60
    jmp isr_common_stub

isr_stub_61:
    pushq $0
    pushq $61
    jmp isr_common_stub

isr_stub_62:
    pushq $0
    pushq $62
    jmp isr_common_stub

isr_stub_63:
    pushq $0
    pushq $63
    jmp isr_common_stub

isr_stub_64:
    pushq $0
    pushq $64
    jmp isr_common_stub

isr_stub_65:
    pushq $0
    pushq $65
    jmp isr_common_stub

isr_stub_66:
    pushq $0
    pushq $66
    jmp isr_common_stub

isr_stub_67:
    pushq $0
    pushq $67
    jmp isr_common_stub

isr_stub_68:
    pushq $0
    pushq $68
    jmp isr_common_stub

isr_stub_69:
    pushq $0
    pushq $69
    jmp isr_common_stub

isr_stub_70:
    pushq $0
    pushq $70
    jmp isr_common_stub

isr_stub_71:
    pushq $0
    pushq $71
    jmp isr_common_stub

isr_stub_72:
    pushq $0
    pushq $72
    jmp isr_common_stub

isr_stub_73:
    pushq $0
    pushq $73
    jmp isr_common_stub

isr_stub_74:
    pushq $0
    pushq $74
    jmp isr_common_stub

isr_stub_75:
    pushq $0
    pushq $75
    jmp isr_common_stub

isr_stub_76:
    pushq $0
    pushq $76
    jmp isr_common_stub

isr_stub_77:
    pushq $0
    pushq $77
    jmp isr_common_stub

isr_stub_78:
    pushq $0
    pushq $78
    jmp isr_common_stub

isr_stub_79:
    pushq $0
    pushq $79
    jmp isr_common_stub

isr_stub_80:
    pushq $0
    pushq $80
    jmp isr_common_stub

isr_stub_81:
    pushq $0
    pushq $81
    jmp isr_common_stub

isr_stub_82:
    pushq $0
    pushq $82
    jmp isr_common_stub

isr_stub_83:
    pushq $0
    pushq $83
    jmp isr_common_stub

isr_stub_84:
    pushq $0
    pushq $84
    jmp isr_common_stub

isr_stub_85:
    pushq $0
    pushq $85
    jmp isr_common_stub

isr_stub_86:
    pushq $0
    pushq $86
    jmp isr_common_stub

isr_stub_87:
    pushq $0
    pushq $87
    jmp isr_common_stub

isr_stub_88:
    pushq $0
    pushq $88
    jmp isr_common_stub

isr_stub_89:
    pushq $0
    pushq $89
    jmp isr_common_stub

isr_stub_90:
    pushq $0
    pushq $90
    jmp isr_common_stub

isr_stub_91:
    pushq $0
    pushq $91
    jmp isr_common_stub

isr_stub_92:
    pushq $0
    pushq $92
    jmp isr_common_stub

isr_stub_93:
    pushq $0
    pushq $93
    jmp isr_common_stub

isr_stub_94:
    pushq $0
    pushq $94
    jmp isr_common_stub

isr_stub_95:
    pushq $0
    pushq $95
    jmp isr_common_stub

isr_stub_96:
    pushq $0
    pushq $96
    jmp isr_common_stub

isr_stub_97:
    pushq $0
    pushq $97
    jmp isr_common_stub

isr_stub_98:
    pushq $0
    pushq $98
    jmp isr_common_stub

isr_stub_99:
    pushq $0
    pushq $99
    jmp isr_common_stub

isr_stub_100:
    pushq $0
    pushq $100
    jmp isr_common_stub

isr_stub_101:
    pushq $0
    pushq $101
    jmp isr_common_stub

isr_stub_102:
    pushq $0
    pushq $102
    jmp isr_common_stub

isr_stub_103:
    pushq $0
    pushq $103
    jmp isr_common_stub

isr_stub_104:
    pushq $0
    pushq $104
    jmp isr_common_stub

isr_stub_105:
    pushq $0
    pushq $105
    jmp isr_common_stub

isr_stub_106:
    pushq $0
    pushq $106
    jmp isr_common_stub

isr_stub_107:
    pushq $0
    pushq $107
    jmp isr_common_stub

isr_stub_108:
    pushq $0
    pushq $108
    jmp isr_common_stub

isr_stub_109:
    pushq $0
    pushq $109
    jmp isr_common_stub

isr_stub_110:
    pushq $0
    pushq $110
    jmp isr_common_stub

isr_stub_111:
    pushq $0
    pushq $111
    jmp isr_common_stub

isr_stub_112:
    pushq $0
    pushq $112
    jmp isr_common_stub

isr_stub_113:
    pushq $0
    pushq $113
    jmp isr_common_stub

isr_stub_114:
    pushq $0
    pushq $114
    jmp isr_common_stub

isr_stub_115:
    pushq $0
    pushq $115
    jmp isr_common_stub

isr_stub_116:
    pushq $0
    pushq $116
    jmp isr_common_stub

isr_stub_117:
    pushq $0
    pushq $117
    jmp isr_common_stub

isr_stub_118:
    pushq $0
    pushq $118
    jmp isr_common_stub

isr_stub_119:
    pushq $0
    pushq $119
    jmp isr_common_stub

isr_stub_120:
    pushq $0
    pushq $120
    jmp isr_common_stub

isr_stub_121:
    pushq $0
    pushq $121
    jmp isr_common_stub

isr_stub_122:
    pushq $0
    pushq $122
    jmp isr_common_stub

isr_stub_123:
    pushq $0
    pushq $123
    jmp isr_common_stub

isr_stub_124:
    pushq $0
    pushq $124
    jmp isr_common_stub

isr_stub_125:
    pushq $0
    pushq $125
    jmp isr_common_stub

isr_stub_126:
    pushq $0
    pushq $126
    jmp isr_common_stub

isr_stub_127:
    pushq $0
    pushq $127
    jmp isr_common_stub

isr_stub_128:
    pushq $0
    pushq $128
    jmp isr_common_stub

isr_stub_129:
    pushq $0
    pushq $129
    jmp isr_common_stub

isr_stub_130:
    pushq $0
    pushq $130
    jmp isr_common_stub

isr_stub_131:
    pushq $0
    pushq $131
    jmp isr_common_stub

isr_stub_132:
    pushq $0
    pushq $132
    jmp isr_common_stub

isr_stub_133:
    pushq $0
    pushq $133
    jmp isr_common_stub

isr_stub_134:
    pushq $0
    pushq $134
    jmp isr_common_stub

isr_stub_135:
    pushq $0
    pushq $135
    jmp isr_common_stub

isr_stub_136:
    pushq $0
    pushq $136
    jmp isr_common_stub

isr_stub_137:
    pushq $0
    pushq $137
    jmp isr_common_stub

isr_stub_138:
    pushq $0
    pushq $138
    jmp isr_common_stub

isr_stub_139:
    pushq $0
    pushq $139
    jmp isr_common_stub

isr_stub_140:
    pushq $0
    pushq $140
    jmp isr_common_stub

isr_stub_141:
    pushq $0
    pushq $141
    jmp isr_common_stub

isr_stub_142:
    pushq $0
    pushq $142
    jmp isr_common_stub

isr_stub_143:
    pushq $0
    pushq $143
    jmp isr_common_stub

isr_stub_144:
    pushq $0
    pushq $144
    jmp isr_common_stub

isr_stub_145:
    pushq $0
    pushq $145
    jmp isr_common_stub

isr_stub_146:
    pushq $0
    pushq $146
    jmp isr_common_stub

isr_stub_147:
    pushq $0
    pushq $147
    jmp isr_common_stub

isr_stub_148:
    pushq $0
    pushq $148
    jmp isr_common_stub

isr_stub_149:
    pushq $0
    pushq $149
    jmp isr_common_stub

isr_stub_150:
    pushq $0
    pushq $150
    jmp isr_common_stub

isr_stub_151:
    pushq $0
    pushq $151
    jmp isr_common_stub

isr_stub_152:
    pushq $0
    pushq $152
    jmp isr_common_stub

isr_stub_153:
    pushq $0
    pushq $153
    jmp isr_common_stub

isr_stub_154:
    pushq $0
    pushq $154
    jmp isr_common_stub

isr_stub_155:
    pushq $0
    pushq $155
    jmp isr_common_stub

isr_stub_156:
    pushq $0
    pushq $156
    jmp isr_common_stub

isr_stub_157:
    pushq $0
    pushq $157
    jmp isr_common_stub

isr_stub_158:
    pushq $0
    pushq $158
    jmp isr_common_stub

isr_stub_159:
    pushq $0
    pushq $159
    jmp isr_common_stub

isr_stub_160:
    pushq $0
    pushq $160
    jmp isr_common_stub

isr_stub_161:
    pushq $0
    pushq $161
    jmp isr_common_stub

isr_stub_162:
    pushq $0
    pushq $162
    jmp isr_common_stub

isr_stub_163:
    pushq $0
    pushq $163
    jmp isr_common_stub

isr_stub_164:
    pushq $0
    pushq $164
    jmp isr_common_stub

isr_stub_165:
    pushq $0
    pushq $165
    jmp isr_common_stub

isr_stub_166:
    pushq $0
    pushq $166
    jmp isr_common_stub

isr_stub_167:
    pushq $0
    pushq $167
    jmp isr_common_stub

isr_stub_168:
    pushq $0
    pushq $168
    jmp isr_common_stub

isr_stub_169:
    pushq $0
    pushq $169
    jmp isr_common_stub

isr_stub_170:
    pushq $0
    pushq $170
    jmp isr_common_stub

isr_stub_171:
    pushq $0
    pushq $171
    jmp isr_common_stub

isr_stub_172:
    pushq $0
    pushq $172
    jmp isr_common_stub

isr_stub_173:
    pushq $0
    pushq $173
    jmp isr_common_stub

isr_stub_174:
    pushq $0
    pushq $174
    jmp isr_common_stub

isr_stub_175:
    pushq $0
    pushq $175
    jmp isr_common_stub

isr_stub_176:
    pushq $0
    pushq $176
    jmp isr_common_stub

isr_stub_177:
    pushq $0
    pushq $177
    jmp isr_common_stub

isr_stub_178:
    pushq $0
    pushq $178
    jmp isr_common_stub

isr_stub_179:
    pushq $0
    pushq $179
    jmp isr_common_stub

isr_stub_180:
    pushq $0
    pushq $180
    jmp isr_common_stub

isr_stub_181:
    pushq $0
    pushq $181
    jmp isr_common_stub

isr_stub_182:
    pushq $0
    pushq $182
    jmp isr_common_stub

isr_stub_183:
    pushq $0
    pushq $183
    jmp isr_common_stub

isr_stub_184:
    pushq $0
    pushq $184
    jmp isr_common_stub

isr_stub_185:
    pushq $0
    pushq $185
    jmp isr_common_stub

isr_stub_186:
    pushq $0
    pushq $186
    jmp isr_common_stub

isr_stub_187:
    pushq $0
    pushq $187
    jmp isr_common_stub

isr_stub_188:
    pushq $0
    pushq $188
    jmp isr_common_stub

isr_stub_189:
    pushq $0
    pushq $189
    jmp isr_common_stub

isr_stub_190:
    pushq $0
    pushq $190
    jmp isr_common_stub

isr_stub_191:
    pushq $0
    pushq $191
    jmp isr_common_stub

isr_stub_192:
    pushq $0
    pushq $192
    jmp isr_common_stub

isr_stub_193:
    pushq $0
    pushq $193
    jmp isr_common_stub

isr_stub_194:
    pushq $0
    pushq $194
    jmp isr_common_stub

isr_stub_195:
    pushq $0
    pushq $195
    jmp isr_common_stub

isr_stub_196:
    pushq $0
    pushq $196
    jmp isr_common_stub

isr_stub_197:
    pushq $0
    pushq $197
    jmp isr_common_stub

isr_stub_198:
    pushq $0
    pushq $198
    jmp isr_common_stub

isr_stub_199:
    pushq $0
    pushq $199
    jmp isr_common_stub

isr_stub_200:
    pushq $0
    pushq $200
    jmp isr_common_stub

isr_stub_201:
    pushq $0
    pushq $201
    jmp isr_common_stub

isr_stub_202:
    pushq $0
    pushq $202
    jmp isr_common_stub

isr_stub_203:
    pushq $0
    pushq $203
    jmp isr_common_stub

isr_stub_204:
    pushq $0
    pushq $204
    jmp isr_common_stub

isr_stub_205:
    pushq $0
    pushq $205
    jmp isr_common_stub

isr_stub_206:
    pushq $0
    pushq $206
    jmp isr_common_stub

isr_stub_207:
    pushq $0
    pushq $207
    jmp isr_common_stub

isr_stub_208:
    pushq $0
    pushq $208
    jmp isr_common_stub

isr_stub_209:
    pushq $0
    pushq $209
    jmp isr_common_stub

isr_stub_210:
    pushq $0
    pushq $210
    jmp isr_common_stub

isr_stub_211:
    pushq $0
    pushq $211
    jmp isr_common_stub

isr_stub_212:
    pushq $0
    pushq $212
    jmp isr_common_stub

isr_stub_213:
    pushq $0
    pushq $213
    jmp isr_common_stub

isr_stub_214:
    pushq $0
    pushq $214
    jmp isr_common_stub

isr_stub_215:
    pushq $0
    pushq $215
    jmp isr_common_stub

isr_stub_216:
    pushq $0
    pushq $216
    jmp isr_common_stub

isr_stub_217:
    pushq $0
    pushq $217
    jmp isr_common_stub

isr_stub_218:
    pushq $0
    pushq $218
    jmp isr_common_stub

isr_stub_219:
    pushq $0
    pushq $219
    jmp isr_common_stub

isr_stub_220:
    pushq $0
    pushq $220
    jmp isr_common_stub

isr_stub_221:
    pushq $0
    pushq $221
    jmp isr_common_stub

isr_stub_222:
    pushq $0
    pushq $222
    jmp isr_common_stub

isr_stub_223:
    pushq $0
    pushq $223
    jmp isr_common_stub

isr_stub_224:
    pushq $0
    pushq $224
    jmp isr_common_stub

isr_stub_225:
    pushq $0
    pushq $225
    jmp isr_common_stub

isr_stub_226:
    pushq $0
    pushq $226
    jmp isr_common_stub

isr_stub_227:
    pushq $0
    pushq $227
    jmp isr_common_stub

isr_stub_228:
    pushq $0
    pushq $228
    jmp isr_common_stub

isr_stub_229:
    pushq $0
    pushq $229
    jmp isr_common_stub

isr_stub_230:
    pushq $0
    pushq $230
    jmp isr_common_stub

isr_stub_231:
    pushq $0
    pushq $231
    jmp isr_common_stub

isr_stub_232:
    pushq $0
    pushq $232
    jmp isr_common_stub

isr_stub_233:
    pushq $0
    pushq $233
    jmp isr_common_stub

isr_stub_234:
    pushq $0
    pushq $234
    jmp isr_common_stub

isr_stub_235:
    pushq $0
    pushq $235
    jmp isr_common_stub

isr_stub_236:
    pushq $0
    pushq $236
    jmp isr_common_stub

isr_stub_237:
    pushq $0
    pushq $237
    jmp isr_common_stub

isr_stub_238:
    pushq $0
    pushq $238
    jmp isr_common_stub

isr_stub_239:
    pushq $0
    pushq $239
    jmp isr_common_stub

isr_stub_240:
    pushq $0
    pushq $240
    jmp isr_common_stub

isr_stub_241:
    pushq $0
    pushq $241
    jmp isr_common_stub

isr_stub_242:
    pushq $0
    pushq $242
    jmp isr_common_stub

isr_stub_243:
    pushq $0
    pushq $243
    jmp isr_common_stub

isr_stub_244:
    pushq $0
    pushq $244
    jmp isr_common_stub

isr_stub_245:
    pushq $0
    pushq $245
    jmp isr_common_stub

isr_stub_246:
    pushq $0
    pushq $246
    jmp isr_common_stub

isr_stub_247:
    pushq $0
    pushq $247
    jmp isr_common_stub

isr_stub_248:
    pushq $0
    pushq $248
    jmp isr_common_stub

isr_stub_249:
    pushq $0
    pushq $249
    jmp isr_common_stub

isr_stub_250:
    pushq $0
    pushq $250
    jmp isr_common_stub

isr_stub_251:
    pushq $0
    pushq $251
    jmp isr_common_stub

isr_stub_252:
    pushq $0
    pushq $252
    jmp isr_common_stub

isr_stub_253:
    pushq $0
    pushq $253
    jmp isr_common_stub

isr_stub_254:
    pushq $0
    pushq $254
    jmp isr_common_stub

isr_stub_255:
    pushq $0
    pushq $255
    jmp isr_common_stub

.extern isr_handler
isr_common_stub:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax
    movq %rsp, %rdi
    cld
    call isr_handler
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    addq $16, %rsp
    iretq

.global isr_stub_table
isr_stub_table:
    .quad isr_stub_0
    .quad isr_stub_1
    .quad isr_stub_2
    .quad isr_stub_3
    .quad isr_stub_4
    .quad isr_stub_5
    .quad isr_stub_6
    .quad isr_stub_7
    .quad isr_stub_8
    .quad isr_stub_9
    .quad isr_stub_10
    .quad isr_stub_11
    .quad isr_stub_12
    .quad isr_stub_13
    .quad isr_stub_14
    .quad isr_stub_15
    .quad isr_stub_16
    .quad isr_stub_17
    .quad isr_stub_18
    .quad isr_stub_19
    .quad isr_stub_20
    .quad isr_stub_21
    .quad isr_stub_22
    .quad isr_stub_23
    .quad isr_stub_24
    .quad isr_stub_25
    .quad isr_stub_26
    .quad isr_stub_27
    .quad isr_stub_28
    .quad isr_stub_29
    .quad isr_stub_30
    .quad isr_stub_31
    .quad isr_stub_32
    .quad isr_stub_33
    .quad isr_stub_34
    .quad isr_stub_35
    .quad isr_stub_36
    .quad isr_stub_37
    .quad isr_stub_38
    .quad isr_stub_39
    .quad isr_stub_40
    .quad isr_stub_41
    .quad isr_stub_42
    .quad isr_stub_43
    .quad isr_stub_44
    .quad isr_stub_45
    .quad isr_stub_46
    .quad isr_stub_47
    .quad isr_stub_48
    .quad isr_stub_49
    .quad isr_stub_50
    .quad isr_stub_51
    .quad isr_stub_52
    .quad isr_stub_53
    .quad isr_stub_54
    .quad isr_stub_55
    .quad isr_stub_56
    .quad isr_stub_57
    .quad isr_stub_58
    .quad isr_stub_59
    .quad isr_stub_60
    .quad isr_stub_61
    .quad isr_stub_62
    .quad isr_stub_63
    .quad isr_stub_64
    .quad isr_stub_65
    .quad isr_stub_66
    .quad isr_stub_67
    .quad isr_stub_68
    .quad isr_stub_69
    .quad isr_stub_70
    .quad isr_stub_71
    .quad isr_stub_72
    .quad isr_stub_73
    .quad isr_stub_74
    .quad isr_stub_75
    .quad isr_stub_76
    .quad isr_stub_77
    .quad isr_stub_78
    .quad isr_stub_79
    .quad isr_stub_80
    .quad isr_stub_81
    .quad isr_stub_82
    .quad isr_stub_83
    .quad isr_stub_84
    .quad isr_stub_85
    .quad isr_stub_86
    .quad isr_stub_87
    .quad isr_stub_88
    .quad isr_stub_89
    .quad isr_stub_90
    .quad isr_stub_91
    .quad isr_stub_92
    .quad isr_stub_93
    .quad isr_stub_94
    .quad isr_stub_95
    .quad isr_stub_96
    .quad isr_stub_97
    .quad isr_stub_98
    .quad isr_stub_99
    .quad isr_stub_100
    .quad isr_stub_101
    .quad isr_stub_102
    .quad isr_stub_103
    .quad isr_stub_104
    .quad isr_stub_105
    .quad isr_stub_106
    .quad isr_stub_107
    .quad isr_stub_108
    .quad isr_stub_109
    .quad isr_stub_110
    .quad isr_stub_111
    .quad isr_stub_112
    .quad isr_stub_113
    .quad isr_stub_114
    .quad isr_stub_115
    .quad isr_stub_116
    .quad isr_stub_117
    .quad isr_stub_118
    .quad isr_stub_119
    .quad isr_stub_120
    .quad isr_stub_121
    .quad isr_stub_122
    .quad isr_stub_123
    .quad isr_stub_124
    .quad isr_stub_125
    .quad isr_stub_126
    .quad isr_stub_127
    .quad isr_stub_128
    .quad isr_stub_129
    .quad isr_stub_130
    .quad isr_stub_131
    .quad isr_stub_132
    .quad isr_stub_133
    .quad isr_stub_134
    .quad isr_stub_135
    .quad isr_stub_136
    .quad isr_stub_137
    .quad isr_stub_138
    .quad isr_stub_139
    .quad isr_stub_140
    .quad isr_stub_141
    .quad isr_stub_142
    .quad isr_stub_143
    .quad isr_stub_144
    .quad isr_stub_145
    .quad isr_stub_146
    .quad isr_stub_147
    .quad isr_stub_148
    .quad isr_stub_149
    .quad isr_stub_150
    .quad isr_stub_151
    .quad isr_stub_152
    .quad isr_stub_153
    .quad isr_stub_154
    .quad isr_stub_155
    .quad isr_stub_156
    .quad isr_stub_157
    .quad isr_stub_158
    .quad isr_stub_159
    .quad isr_stub_160
    .quad isr_stub_161
    .quad isr_stub_162
    .quad isr_stub_163
    .quad isr_stub_164
    .quad isr_stub_165
    .quad isr_stub_166
    .quad isr_stub_167
    .quad isr_stub_168
    .quad isr_stub_169
    .quad isr_stub_170
    .quad isr_stub_171
    .quad isr_stub_172
    .quad isr_stub_173
    .quad isr_stub_174
    .quad isr_stub_175
    .quad isr_stub_176
    .quad isr_stub_177
    .quad isr_stub_178
    .quad isr_stub_179
    .quad isr_stub_180
    .quad isr_stub_181
    .quad isr_stub_182
    .quad isr_stub_183
    .quad isr_stub_184
    .quad isr_stub_185
    .quad isr_stub_186
    .quad isr_stub_187
    .quad isr_stub_188
    .quad isr_stub_189
    .quad isr_stub_190
    .quad isr_stub_191
    .quad isr_stub_192
    .quad isr_stub_193
    .quad isr_stub_194
    .quad isr_stub_195
    .quad isr_stub_196
    .quad isr_stub_197
    .quad isr_stub_198
    .quad isr_stub_199
    .quad isr_stub_200
    .quad isr_stub_201
    .quad isr_stub_202
    .quad isr_stub_203
    .quad isr_stub_204
    .quad isr_stub_205
    .quad isr_stub_206
    .quad isr_stub_207
    .quad isr_stub_208
    .quad isr_stub_209
    .quad isr_stub_210
    .quad isr_stub_211
    .quad isr_stub_212
    .quad isr_stub_213
    .quad isr_stub_214
    .quad isr_stub_215
    .quad isr_stub_216
    .quad isr_stub_217
    .quad isr_stub_218
    .quad isr_stub_219
    .quad isr_stub_220
    .quad isr_stub_221
    .quad isr_stub_222
    .quad isr_stub_223
    .quad isr_stub_224
    .quad isr_stub_225
    .quad isr_stub_226
    .quad isr_stub_227
    .quad isr_stub_228
    .quad isr_stub_229
    .quad isr_stub_230
    .quad isr_stub_231
    .quad isr_stub_232
    .quad isr_stub_233
    .quad isr_stub_234
    .quad isr_stub_235
    .quad isr_stub_236
    .quad isr_stub_237
    .quad isr_stub_238
    .quad isr_stub_239
    .quad isr_stub_240
    .quad isr_stub_241
    .quad isr_stub_242
    .quad isr_stub_243
    .quad isr_stub_244
    .quad isr_stub_245
    .quad isr_stub_246
    .quad isr_stub_247
    .quad isr_stub_248
    .quad isr_stub_249
    .quad isr_stub_250
    .quad isr_stub_251
    .quad isr_stub_252
    .quad isr_stub_253
    .quad isr_stub_254
    .quad isr_stub_255
