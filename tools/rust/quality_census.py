#!/usr/bin/env python3
"""Generate the observational C++ ↔ Rust migration quality census."""
import argparse, datetime as dt, json, math, os, re, shutil, statistics, subprocess
from collections import defaultdict
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
CPP_EXT={".c",".cc",".cpp",".cxx",".h",".hh",".hpp",".hxx"}
TEST_MARKERS=("/tests/","/test/","/ut_","/ut/","tests.cpp","test.cpp","tests.cxx","test.cxx")

def load(p):
    with Path(p).open(encoding="utf-8-sig") as f:return json.load(f)
def dump(p,v):
    p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(json.dumps(v,indent=2,sort_keys=True)+"\n",encoding="utf-8")
def run(a):
    return subprocess.run(a,cwd=ROOT,check=True,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout
def num(d,*ks):
    x=d
    for k in ks:
        if not isinstance(x,dict) or k not in x:return None
        x=x[k]
    return float(x) if isinstance(x,(int,float)) and math.isfinite(float(x)) else None
def ratio(a,b):return None if a is None or b in (None,0) else a/b
def pct(v,q=.95):
    if not v:return None
    v=sorted(v);return v[max(0,min(len(v)-1,math.ceil(q*len(v))-1))]
def stats(v):
    v=[float(x) for x in v if isinstance(x,(int,float)) and math.isfinite(float(x))]
    return {"count":len(v),"mean":statistics.fmean(v) if v else None,
            "median":statistics.median(v) if v else None,"p95":pct(v),"max":max(v) if v else None}
def f(v):
    if v is None:return "n/a"
    if isinstance(v,int):return f"{v:,}"
    if isinstance(v,float):return f"{v:,.2f}"
    return str(v)

def product_cpp(path,role=""):
    p=path.replace("\\","/");low=p.lower();r=role.lower()
    if Path(p).suffix.lower() not in CPP_EXT:return False
    if any(x in r for x in ("contract","test","vector","fixture")):return False
    if r and any(x in r for x in ("product","reference implementation","implementation","owner")):return True
    return not any(x in low for x in TEST_MARKERS)

def owners(e):
    out=[]
    if isinstance(e.get("rustPath"),str) and e["rustPath"].endswith(".rs"):out.append(e["rustPath"])
    for o in e.get("rustOwners",[]) or []:
        if isinstance(o,dict) and isinstance(o.get("rustPath"),str) and o["rustPath"].endswith(".rs"):out.append(o["rustPath"])
    return out

def copy(paths,dst):
    out=[]
    for rel in sorted(set(paths)):
        src=ROOT/rel
        if not src.is_file():raise RuntimeError(f"Mapped source missing: {rel}")
        target=dst/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,target);out.append(rel)
    return out

def scope(root):
    cpp,rust=set(),set();excluded=[];used=[]
    for mp in sorted((ROOT/"tools/rust").glob("*source-map*.json")):
        d=load(mp);before=(len(cpp),len(rust))
        if not isinstance(d,dict):continue
        for e in d.get("entries",[]) or []:
            if not isinstance(e,dict) or not isinstance(e.get("sourcePath"),str):continue
            s=e["sourcePath"];o=owners(e)
            if e.get("ownership")=="native":excluded.append({"source":s,"reason":"native ownership boundary"});continue
            if not o:continue
            if product_cpp(s,str(e.get("role",""))):cpp.add(s);rust.update(o)
            else:excluded.append({"source":s,"reason":"contract/test source excluded"})
        ro=d.get("rustOwner")
        if isinstance(ro,str) and ro.endswith(".rs"):
            for e in d.get("sources",[]) or []:
                if not isinstance(e,dict) or not isinstance(e.get("sourcePath"),str):continue
                s=e["sourcePath"];role=str(e.get("role",""))
                if product_cpp(s,role):cpp.add(s);rust.add(ro)
                else:excluded.append({"source":s,"reason":f"source-map role excluded: {role or 'non-product'}"})
        if before!=(len(cpp),len(rust)):used.append(str(mp.relative_to(ROOT)).replace("\\","/"))
    if not cpp or not rust:raise RuntimeError(f"Comparable scope empty: C++={len(cpp)} Rust={len(rust)}")
    st=root/"staging"
    if st.exists():shutil.rmtree(st)
    cp=copy(cpp,st/"cpp-comparable");rp=copy(rust,st/"rust-comparable")
    prod=[str(p.relative_to(ROOT)).replace("\\","/") for p in ROOT.glob("rust/*/src/**/*.rs") if p.is_file()]
    tests=[str(p.relative_to(ROOT)).replace("\\","/") for p in ROOT.glob("rust/*/tests/**/*.rs") if p.is_file()]
    copy(prod,st/"rust-production");(st/"rust-tests").mkdir(parents=True,exist_ok=True)
    if tests:copy(tests,st/"rust-tests")
    result={"schemaVersion":1,
      "basis":"Product C/C++ sources explicitly mapped to Rust owners by repository source maps; native-only and Microsoft contract/test-vector sources are excluded.",
      "sourceMaps":used,"cppProductSources":cp,"rustComparableOwners":rp,"excludedSources":excluded,
      "rustWorkspace":"rust/** (all Cargo workspace crates)",
      "productionTestClassification":{"production":"rust/*/src/**/*.rs","tests":"rust/*/tests/**/*.rs",
      "caveat":"Inline #[cfg(test)] modules remain in source files and therefore remain in production file SLOC."}}
    dump(root/"comparable-scope.json",result);return result

def rca(d):
    files=sorted(Path(d).rglob("*.json"))
    if not files:raise RuntimeError(f"No rust-code-analysis JSON in {d}")
    loc=defaultdict(float);nfun=0.;mis=[];effort=0.;rows=[]
    def walk(spaces,src,prefix=""):
        for s in spaces if isinstance(spaces,list) else []:
            if not isinstance(s,dict):continue
            m=s.get("metrics",{});kind=str(s.get("kind","space"));name=str(s.get("name","<anonymous>"))
            row={"name":f"{src}::{prefix}{name}","kind":kind,"cyclomatic":num(m,"cyclomatic","sum"),
                 "cognitive":num(m,"cognitive","sum"),"lloc":num(m,"loc","lloc"),
                 "maintainabilityIndex":num(m,"mi","mi_visual_studio")}
            if kind.lower() not in {"unit","namespace","impl","class","trait","module"} and (row["cyclomatic"] is not None or row["cognitive"] is not None):rows.append(row)
            walk(s.get("spaces"),src,prefix+name+"::")
    for p in files:
        d=load(p)
        if not isinstance(d,dict):continue
        m=d.get("metrics",{});lm=m.get("loc",{}) if isinstance(m,dict) else {}
        for k in ("sloc","ploc","lloc","cloc","blank"):
            if isinstance(lm.get(k),(int,float)):loc[k]+=lm[k]
        if isinstance(m.get("nom",{}).get("total"),(int,float)):nfun+=m["nom"]["total"]
        mi=num(m,"mi","mi_visual_studio");w=num(m,"loc","lloc") or 1.
        if mi is not None:mis.append((mi,max(w,1.)))
        e=num(m,"halstead","effort")
        if e is not None:effort+=e
        walk(d.get("spaces"),str(d.get("name",p.name)))
    cc=[x["cyclomatic"] for x in rows if x["cyclomatic"] is not None]
    cg=[x["cognitive"] for x in rows if x["cognitive"] is not None]
    hot=sorted(rows,key=lambda x:(x["cognitive"] or -1,x["cyclomatic"] or -1,x["lloc"] or -1),reverse=True)[:20]
    return {"files":len(files),"functionsMethodsClosures":int(nfun),
      "loc":{k:int(round(loc[k])) for k in ("sloc","ploc","lloc","cloc","blank")},
      "cyclomatic":stats(cc),"cognitive":stats(cg),
      "maintainabilityIndex":{"weightedMeanByLloc":sum(x*w for x,w in mis)/sum(w for _,w in mis) if mis else None,"fileCount":len(mis)},
      "halstead":{"totalEffort":effort},"hotspots":hot}

def tokei(p,languages):
    d=load(p);t=defaultdict(int);files=0
    for lang,m in d.items():
        if lang=="Total" or lang not in languages or not isinstance(m,dict):continue
        for k in ("lines","code","comments","blanks"):
            if isinstance(m.get(k),int):t[k]+=m[k]
        if isinstance(m.get("reports"),list):files+=len(m["reports"])
    return {"files":files,**dict(t)}

def functional(g,d):
    gt=Path(g).read_text(encoding="utf-8",errors="replace");dtxt=Path(d).read_text(encoding="utf-8",errors="replace")
    cm=re.search(r"Microsoft global coverage:\s*(.+)",gt);tm=re.search(r"inventory gate passed \((\d+) source methods",gt)
    dm=re.search(r"R08 Partial debt:\s*total=(\d+);\s*(.+);\s*Missing=(\d+)",dtxt)
    if not(cm and tm and dm):raise RuntimeError("Cannot parse authoritative R08 census output")
    cov={k.strip():int(v) for k,v in (x.strip().split("=",1) for x in cm.group(1).split(","))}
    cls={k.strip():int(v) for k,v in (x.strip().split("=",1) for x in dm.group(2).split(","))}
    exact,strong,missing,fp=cov.get("Exact",0),cov.get("Stronger",0),cov.get("Missing",0),cls.get("functional",0)
    universe=exact+strong+missing+fp
    return {"microsoftContractsTotal":int(tm.group(1)),"functionalUniverse":universe,"Exact":exact,"Stronger":strong,
      "Partial":cov.get("Partial",0),"FunctionalPartial":fp,"PlatformOnly":cov.get("Platform-only",0),
      "UIManaged":cov.get("UI-managed",0),"Missing":missing,"partialClasses":cls,
      "functionalCoveragePercent":100*(exact+strong)/universe if universe else None,
      "source":{"globalInventory":"tools/rust/Test-MicrosoftGlobalTestInventory.ps1","functionalDebt":"tools/rust/Test-R08FunctionalDebt.ps1"}}

def coverage(p):
    d=load(p)
    try:t=d["data"][0]["totals"]
    except Exception as e:raise RuntimeError("cargo-llvm-cov JSON lacks data[0].totals") from e
    out={}
    for k in ("lines","regions","functions"):
        if not isinstance(t.get(k),dict):raise RuntimeError(f"coverage totals missing {k}")
        out[k]={x:t[k].get(x) for x in ("covered","count","percent")}
    out["branches"]={"status":"not-measured","reason":"cargo-llvm-cov --branch is documented as unstable and is not enabled in the stable-toolchain baseline."}
    return out

NODE=re.compile(r'^\s*"([^"]+)"\s*\[');EDGE=re.compile(r'^\s*"([^"]+)"\s*->\s*"([^"]+)"')
def scc(nodes,edges):
    adj=defaultdict(list)
    for a,b in edges:adj[a].append(b)
    idx=0;stack=[];on=set();ind={};low={};out=[]
    def visit(v):
        nonlocal idx
        ind[v]=low[v]=idx;idx+=1;stack.append(v);on.add(v)
        for w in adj[v]:
            if w not in ind:visit(w);low[v]=min(low[v],low[w])
            elif w in on:low[v]=min(low[v],ind[w])
        if low[v]==ind[v]:
            c=[]
            while True:
                w=stack.pop();on.remove(w);c.append(w)
                if w==v:break
            out.append(c)
    for v in sorted(nodes):
        if v not in ind:visit(v)
    return out
def coupling(d):
    dots=sorted(Path(d).glob("*.dot"))
    if not dots:raise RuntimeError("No cargo-modules DOT output")
    nodes=set();edges=set()
    for p in dots:
        pre=p.stem
        for line in p.read_text(encoding="utf-8",errors="replace").splitlines():
            m=EDGE.match(line)
            if m:
                a,b=f"{pre}::{m.group(1)}",f"{pre}::{m.group(2)}";nodes|={a,b};edges.add((a,b));continue
            m=NODE.match(line)
            if m:nodes.add(f"{pre}::{m.group(1)}")
    inc=defaultdict(int);out=defaultdict(int)
    for a,b in edges:out[a]+=1;inc[b]+=1
    rec=[]
    for n in nodes:
        ca,ce=inc[n],out[n];tot=ca+ce
        rec.append({"module":n,"fanIn":ca,"fanOut":ce,"Ca":ca,"Ce":ce,"instability":ce/tot if tot else 0.,"totalCoupling":tot})
    rec.sort(key=lambda x:(x["totalCoupling"],x["fanOut"],x["fanIn"]),reverse=True)
    cyc=[c for c in scc(nodes,edges) if len(c)>1 or any((x,x) in edges for x in c)]
    return {"modules":len(nodes),"edges":len(edges),"cycles":len(cyc),
      "cycleDefinition":"strongly connected cyclic components (not all elementary cycles)",
      "meanFanIn":statistics.fmean([x["fanIn"] for x in rec]) if rec else 0.,
      "meanFanOut":statistics.fmean([x["fanOut"] for x in rec]) if rec else 0.,
      "coupling":stats([x["totalCoupling"] for x in rec]),"instability":stats([x["instability"] for x in rec]),
      "hotspots":rec[:20],"cyclicComponents":[sorted(c) for c in cyc[:20]]}

def tool_versions(raw,manifest):
    observed={};p=raw/"tool-versions.txt"
    if p.exists():
        for line in p.read_text(encoding="utf-8",errors="replace").splitlines():
            if "=" in line:
                k,v=line.split("=",1);observed[k.strip()]=v.strip()
    return {"declared":manifest,"observed":observed}

def markdown(c):
    fn=c["functionalCensus"];rv=c["rust"]["runtimeCoverage"];r=c["rust"]["comparable"];x=c["cpp"]["comparable"];cp=c["rust"]["coupling"];cmp=c["comparative"]
    L=["# RUST MIGRATION QUALITY CENSUS","",f"Commit: `{c['git']['commitSha']}`  ",f"Ref: `{c['git']['ref']}`  ",f"Generated: {c['generatedAt']}","",
       "## Functional parity","","| Metric | Value |","|---|---:|",
       f"| Microsoft contracts | {f(fn['microsoftContractsTotal'])} |",f"| Functional universe | {f(fn['functionalUniverse'])} |",
       f"| Exact | {f(fn['Exact'])} |",f"| Stronger | {f(fn['Stronger'])} |",f"| Partial | {f(fn['Partial'])} |",
       f"| Functional Partial | {f(fn['FunctionalPartial'])} |",f"| Platform-only | {f(fn['PlatformOnly'])} |",
       f"| UI-managed | {f(fn['UIManaged'])} |",f"| Missing | {f(fn['Missing'])} |",f"| Functional coverage | {f(fn['functionalCoveragePercent'])}% |","",
       "## Runtime coverage — Rust","","| Metric | Coverage |","|---|---:|",
       f"| Lines | {f(rv['lines']['percent'])}% |",f"| Regions | {f(rv['regions']['percent'])}% |",f"| Functions | {f(rv['functions']['percent'])}% |",
       "| Branches | not measured (unstable cargo-llvm-cov option) |","",
       "C++ runtime coverage: **unavailable/not measured**. Microsoft contractual coverage remains the functional reference; no synthetic C++ runtime percentage is produced.","",
       "## Effective size — comparable mapped product scope","","| Metric | C++ | Rust | Rust/C++ |","|---|---:|---:|---:|"]
    for k,label in (("lloc","LLOC"),("sloc","SLOC"),("ploc","PLOC"),("cloc","Comment LOC"),("blank","Blank LOC")):
        L.append(f"| {label} | {f(x['loc'][k])} | {f(r['loc'][k])} | {f(ratio(r['loc'][k],x['loc'][k]))} |")
    L += [f"| Files | {f(x['files'])} | {f(r['files'])} | — |",f"| Functions/methods/closures | {f(x['functionsMethodsClosures'])} | {f(r['functionsMethodsClosures'])} | — |","",
      f"**Effective code compression:** Rust/C++ LLOC = {f(cmp['effectiveCodeCompression']['rustToCppLlocRatio'])}; reduction = {f(cmp['effectiveCodeCompression']['reductionPercent'])}%.","",
      "## Complexity — function/method level","","| Metric | C++ | Rust | Rust/C++ |","|---|---:|---:|---:|"]
    for m,label in (("cyclomatic","Cyclomatic"),("cognitive","Cognitive")):
        for s in ("mean","median","p95","max"):L.append(f"| {label} {s} | {f(x[m][s])} | {f(r[m][s])} | {f(ratio(r[m][s],x[m][s]))} |")
    L += ["","## Maintainability","","| Metric | C++ | Rust | Delta / ratio |","|---|---:|---:|---:|",
      f"| Maintainability Index (LLOC-weighted file mean) | {f(x['maintainabilityIndex']['weightedMeanByLloc'])} | {f(r['maintainabilityIndex']['weightedMeanByLloc'])} | Δ {f(cmp['maintainability']['miDelta'])} |",
      f"| Halstead effort (sum) | {f(x['halstead']['totalEffort'])} | {f(r['halstead']['totalEffort'])} | {f(cmp['maintainability']['halsteadEffortRatio'])} |","",
      "## Rust production / tests","",f"- Production SLOC (file-oriented): **{f(c['rust']['productionTest']['productionSloc'])}**",
      f"- Integration-test SLOC: **{f(c['rust']['productionTest']['testSloc'])}**",
      f"- Test / production SLOC ratio: **{f(c['rust']['productionTest']['testToProductionRatio'])}**",
      "- Inline `#[cfg(test)]` modules remain on the production-file side because this ratio classifies files, not AST fragments.","",
      "## Rust coupling","","| Metric | Value |","|---|---:|",f"| Modules | {f(cp['modules'])} |",f"| Dependency edges | {f(cp['edges'])} |",
      f"| Cyclic SCCs | {f(cp['cycles'])} |",f"| Mean fan-in / Ca | {f(cp['meanFanIn'])} |",f"| Mean fan-out / Ce | {f(cp['meanFanOut'])} |",
      f"| p95 total coupling | {f(cp['coupling']['p95'])} |",f"| Max total coupling | {f(cp['coupling']['max'])} |","",
      "Instability is `Ce / (Ca + Ce)` and is recorded per module in `quality-census.json`.","",
      "## Top complexity hotspots","","| Language | Function / method | Cognitive | Cyclomatic | LLOC |","|---|---|---:|---:|---:|"]
    for lang,m in (("Rust",r),("C++",x)):
        for h in m["hotspots"][:10]:L.append(f"| {lang} | `{str(h['name']).replace('|','&#124;')}` | {f(h.get('cognitive'))} | {f(h.get('cyclomatic'))} | {f(h.get('lloc'))} |")
    L += ["","## Top Rust coupling hotspots","","| Module | Ca | Ce | Instability | Total |","|---|---:|---:|---:|---:|"]
    for h in cp["hotspots"][:20]:L.append(f"| `{h['module'].replace('|','&#124;')}` | {h['Ca']} | {h['Ce']} | {f(h['instability'])} | {h['totalCoupling']} |")
    L += ["","## Scope notes","",f"- Rust runtime/static workspace: `{c['scope']['rustWorkspace']}`.",
      "- C++ ↔ Rust comparison uses only product C/C++ sources explicitly mapped to Rust owners by repository source maps.",
      "- Native-only C++ boundaries and Microsoft C++ test/contract-vector files are excluded from structural compression ratios.",
      f"- Comparable C++ files: {len(c['scope']['cppProductSources'])}; comparable Rust owner files: {len(c['scope']['rustComparableOwners'])}.",
      "- This workflow is observational: no coverage, complexity, coupling, LOC or MI thresholds are enforced."]
    return "\n".join(L)+"\n"

def validate(path):
    d=load(path)
    if d.get("schemaVersion")!=1 or not isinstance(d.get("tools"),dict):raise RuntimeError("Unsupported quality-census-tools schema")
    req={"cargo-llvm-cov","rust-code-analysis-cli","tokei","cargo-modules"}
    if req-set(d["tools"]):raise RuntimeError(f"Missing tools: {sorted(req-set(d['tools']))}")
    for n in req:
        x=d["tools"][n]
        if not re.fullmatch(r"\d+\.\d+\.\d+",str(x.get("version",""))) or "--version" not in x.get("install",[]):raise RuntimeError(f"{n} is not exactly pinned")
    print("Quality census tool manifest is valid.")

def do_coupling(root):
    out=root/"raw/coupling"
    if out.exists():shutil.rmtree(out)
    out.mkdir(parents=True,exist_ok=True)
    md=json.loads(run(["cargo","metadata","--no-deps","--format-version","1"]));members=set(md["workspace_members"]);count=0
    for p in md["packages"]:
        if p["id"] not in members or not any("lib" in t.get("kind",[]) for t in p.get("targets",[])):continue
        crate=p["name"]
        cmd=["cargo","modules","dependencies","-p",crate,"--lib","--no-externs","--no-fns","--no-sysroot","--no-traits","--no-types","--no-uses"]
        count+=1
        try:
            dot=run(cmd)
        except subprocess.CalledProcessError as e:
            diagnostic="\n".join((
                f"crate: {crate}",
                f"command: {json.dumps(cmd)}",
                f"exit_code: {e.returncode}",
                "stdout:",
                e.stdout or "<empty>",
                "stderr:",
                e.stderr or "<empty>",
            ))+"\n"
            error_path=out/f"{crate}.error.txt"
            error_path.write_text(diagnostic,encoding="utf-8")
            print(diagnostic,flush=True)
            raise RuntimeError(f"cargo-modules failed for crate {crate} with exit code {e.returncode}; see {error_path}") from e
        (out/f"{crate}.dot").write_text(dot,encoding="utf-8")
    if not count:raise RuntimeError("cargo-modules found no workspace library crates")
    print(f"Generated module graphs for {count} crates.")

def aggregate(root,output,summary):
    raw=root/"raw";manifest=load(ROOT/"tools/rust/quality-census-tools.json");sc=load(root/"comparable-scope.json")
    fn=functional(raw/"functional-global.log",raw/"functional-debt.log");rw=rca(raw/"rca-rust-workspace");r=rca(raw/"rca-rust-comparable");x=rca(raw/"rca-cpp-comparable")
    prod=rca(raw/"rca-rust-production");tests=rca(raw/"rca-rust-tests");rv=coverage(raw/"coverage.json");cp=coupling(raw/"coupling")
    tw=tokei(raw/"tokei-rust-workspace.json",{"Rust"});tr=tokei(raw/"tokei-rust-comparable.json",{"Rust"});tx=tokei(raw/"tokei-cpp-comparable.json",{"C","Cpp","CHeader","CppHeader"})
    rl,xl=r["loc"]["lloc"],x["loc"]["lloc"]
    if rl<=0 or xl<=0 or fn["functionalUniverse"]<=0 or cp["modules"]<=0:raise RuntimeError("Invalid zero-sized census component")
    mi_r=r["maintainabilityIndex"]["weightedMeanByLloc"];mi_x=x["maintainabilityIndex"]["weightedMeanByLloc"]
    sha=os.getenv("QUALITY_COMMIT_SHA") or run(["git","rev-parse","HEAD"]).strip()
    c={"schemaVersion":1,"generatedAt":dt.datetime.now(dt.timezone.utc).isoformat().replace("+00:00","Z"),
      "git":{"commitSha":sha,"ref":os.getenv("GITHUB_REF",""),"refName":os.getenv("GITHUB_REF_NAME",""),"event":os.getenv("GITHUB_EVENT_NAME",""),"repository":os.getenv("GITHUB_REPOSITORY","ChicoDotNet/terminal")},
      "toolVersions":tool_versions(raw,manifest),"scope":sc,"functionalCensus":fn,
      "rust":{"runtimeCoverage":rv,"workspace":rw,"comparable":r,"tokeiWorkspace":tw,"tokeiComparable":tr,
        "productionTest":{"productionSloc":prod["loc"]["sloc"],"testSloc":tests["loc"]["sloc"],"testToProductionRatio":ratio(tests["loc"]["sloc"],prod["loc"]["sloc"]),"classification":sc["productionTestClassification"]},"coupling":cp},
      "cpp":{"runtimeCoverage":manifest["coveragePolicy"]["cpp"],"comparable":x,"tokeiComparable":tx},
      "comparative":{"effectiveCodeCompression":{"rustToCppLlocRatio":ratio(rl,xl),"reductionPercent":100*(1-rl/xl)},
        "cyclomaticRatio":{s:ratio(r["cyclomatic"][s],x["cyclomatic"][s]) for s in ("mean","p95","max")},
        "cognitiveRatio":{s:ratio(r["cognitive"][s],x["cognitive"][s]) for s in ("mean","p95","max")},
        "maintainability":{"miDelta":mi_r-mi_x if mi_r is not None and mi_x is not None else None,"halsteadEffortRatio":ratio(r["halstead"]["totalEffort"],x["halstead"]["totalEffort"])}}}
    dump(output,c)
    if load(output).get("git",{}).get("commitSha")!=sha:raise RuntimeError("quality-census.json self-check failed")
    Path(summary).write_text(markdown(c),encoding="utf-8");print(markdown(c))

def main():
    p=argparse.ArgumentParser();s=p.add_subparsers(dest="cmd",required=True)
    x=s.add_parser("validate-tools");x.add_argument("--path",required=True)
    x=s.add_parser("scope");x.add_argument("--artifact-root",required=True)
    x=s.add_parser("coupling");x.add_argument("--artifact-root",required=True)
    x=s.add_parser("aggregate");x.add_argument("--artifact-root",required=True);x.add_argument("--output",required=True);x.add_argument("--summary",required=True)
    a=p.parse_args()
    if a.cmd=="validate-tools":validate(a.path)
    elif a.cmd=="scope":
        r=scope(Path(a.artifact_root).resolve());print(f"Comparable scope: C++={len(r['cppProductSources'])}, Rust={len(r['rustComparableOwners'])}")
    elif a.cmd=="coupling":do_coupling(Path(a.artifact_root).resolve())
    else:aggregate(Path(a.artifact_root).resolve(),Path(a.output).resolve(),Path(a.summary).resolve())
if __name__=="__main__":main()
