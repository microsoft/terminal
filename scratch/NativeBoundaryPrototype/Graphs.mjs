// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';
import assert from 'node:assert/strict';

const median = values => {
    const sorted = [...values].sort((a, b) => a - b);
    const middle = Math.floor(sorted.length / 2);
    return sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
};
const escape = text => String(text).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('"', '&quot;');
const changeLabel = percent => `${Math.abs(percent).toFixed(1)}% ${percent < 0 ? 'higher' : 'lower'} median`;
const colors = ['#536dfe', '#e68100', '#00897b'];

function render(data, output) {
    assert(Array.isArray(data.records) && data.records.length >= 2, 'Need successful measurements for both variants');
    const variants = data.variants ?? ['projected', 'native'];
    assert(variants.length >= 2 && variants.length <= colors.length, 'Expected two or three variants');
    assert.equal(new Set(variants).size, variants.length, 'Duplicate variant names');
    const groups = variants.map(variant => {
        const records = data.records.filter(record => record.variant === variant);
        assert(records.length > 0, `No measurements for ${variant}`);
        for (const record of records) {
            assert(Number.isFinite(record.buildSeconds) && record.buildSeconds > 0, 'Invalid wall time');
            assert(Array.isArray(record.phases), 'Missing phase data');
            for (const phase of record.phases) {
                assert(Number.isFinite(phase.seconds) && phase.seconds >= 0, 'Invalid phase duration');
            }
        }
        return {
            name: variant,
            records,
            wall: median(records.map(record => record.buildSeconds)),
            phases: (data.phaseNames ?? ['projection', 'compile', 'archive', 'link']).map(phase => ({
                name: phase,
                seconds: median(records.map(record =>
                    record.phases.filter(item => item.phase === phase).reduce((sum, item) => sum + item.seconds, 0)))
            }))
        };
    });
    const height = 210 + groups.length * 95;
    const start = (title, subtitle) => `<svg xmlns="http://www.w3.org/2000/svg" width="1040" height="${height}" viewBox="0 0 1040 ${height}">
<rect width="1040" height="${height}" fill="#fafafa"/>
<g font-family="Segoe UI, sans-serif" fill="#222">
<text x="30" y="36" font-size="22" font-weight="600">${escape(title)}</text>
<text x="30" y="63" font-size="14">${escape(subtitle)}</text>`;
    const end = '</g></svg>';
    const max = Math.max(...groups.flatMap(group => group.records.map(record => record.buildSeconds))) * 1.15;
    const scale = 650 / max;
    let wall = start(data.title, data.subtitle);
    for (let i = 0; i < groups.length; ++i) {
        const group = groups[i], y = 115 + i * 95;
        wall += `<text x="30" y="${y + 28}" font-size="16">${group.name}</text>
<rect x="170" y="${y}" width="${group.wall * scale}" height="45" fill="${colors[i]}"/>
<text x="${180 + group.wall * scale}" y="${y + 28}" font-size="16">${group.wall.toFixed(2)} s median</text>`;
        for (const record of group.records) {
            wall += `<circle cx="${170 + record.buildSeconds * scale}" cy="${y + 53}" r="4" fill="#222"/>`;
        }
    }
    const improvement = (1 - groups.at(-1).wall / groups[0].wall) * 100;
    wall += `<text x="30" y="${height - 65}" font-size="16">${changeLabel(improvement)}, ${groups[0].name} to ${groups.at(-1).name}; dots = individual runs</text>
<text x="30" y="${height - 35}" font-size="13">${escape(data.footer)}</text>`;
    writeFileSync(join(output, 'build-times.svg'), wall + end);

    let phases = start(data.phasesTitle ?? 'Where the time went',
        data.phasesSubtitle ?? 'Medians of measured serial tool time; phase medians need not sum to median wall time');
    const phaseColors = ['#536dfe', '#00897b', '#f9a825', '#d81b60', '#8e44ad'];
    const phaseMax = Math.max(...groups.map(group => group.phases.reduce((sum, phase) => sum + phase.seconds, 0))) * 1.1;
    for (let i = 0; i < groups.length; ++i) {
        const group = groups[i], y = 115 + i * 95;
        phases += `<text x="30" y="${y + 28}" font-size="16">${group.name}</text>`;
        let x = 170;
        group.phases.forEach((phase, index) => {
            const width = phase.seconds * 700 / phaseMax;
            phases += `<rect x="${x}" y="${y}" width="${width}" height="45" fill="${phaseColors[index]}"><title>${phase.name}: ${phase.seconds.toFixed(3)} s</title></rect>`;
            x += width;
        });
    }
    groups[0].phases.forEach((phase, i) => {
        const x = 30 + i * (980 / groups[0].phases.length);
        phases += `<rect x="${x}" y="${height - 100}" width="16" height="16" fill="${phaseColors[i]}"/>
<text x="${x + 24}" y="${height - 87}" font-size="13">${phase.name}: ${groups.map(group => group.phases[i].seconds.toFixed(2)).join(' / ')} s</text>`;
    });
    phases += `<text x="30" y="${height - 35}" font-size="13">${escape(data.phasesNote)}</text>`;
    writeFileSync(join(output, 'build-phases.svg'), phases + end);
    writeFileSync(join(output, 'summary.json'), JSON.stringify({ scope: data.scope, improvementPercent: improvement, groups }, null, 2));
    console.log(groups.map(group => `${group.name}: ${group.wall.toFixed(3)} s median`).join('\n'));
}

if (process.argv[2] === '--self-test') {
    assert.equal(median([3, 1, 2]), 2);
    assert.equal(median([2, 4]), 3);
    assert.equal(escape('<&"'), '&lt;&amp;&quot;');
    assert.equal(changeLabel(-10.59), '10.6% higher median');
    assert.equal(changeLabel(14.6), '14.6% lower median');
    assert.throws(() => render({ records: [] }), /Need successful measurements/);
    assert.throws(() => render({ records: [{ variant: 'projected', buildSeconds: -1 }], variants: ['projected', 'native'] }), /Need successful measurements/);
    console.log('Graph aggregation self-tests passed');
} else {
    const [input, output] = process.argv.slice(2);
    assert(input && output, 'Usage: node Graphs.mjs measurements.json output-directory');
    mkdirSync(output, { recursive: true });
    render(JSON.parse(readFileSync(input, 'utf8').replace(/^\uFEFF/, '')), output);
}
